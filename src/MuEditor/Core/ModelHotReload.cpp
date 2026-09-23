#include "stdafx.h"

#ifdef _EDITOR

#include "ModelHotReload.h"

#include "Assets/EditorText.h"
#include "Assets/ModelPreflight.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "Core/EditorFiles.h" // ReadWholeFile, PathToUtf8
#include "UI/MapEditor/MapObjectPlace.h"    // LiveObjectsOfType
#include "UI/MapEditor/ObjectThumbnail.h"

#include "Core/Globals/_enum.h"           // MODEL_WORLD_OBJECT, MAX_WORLD_OBJECTS, MAX_MODELS
#include "Core/Globals/_TextureIndex.h"   // BITMAP_HIDE, BITMAP_UNKNOWN, BITMAP_NONAMED_TEXTURES_*
#include "Engine/Object/w_ObjectInfo.h"   // OBJECT
#include "Render/Models/ZzzBMD.h"         // Models, MAX_MESH, MAX_VERTICES, MAX_BONES
#include "Render/Sprites/GlobalBitmap.h"  // Bitmaps
#include "Render/Terrain/ZzzLodTerrain.h" // MapFileDecrypt
#include "World/MapInfra/MapManager.h"    // gMapManager.WorldActive

#include <algorithm>
#include <cwchar>
#include <unordered_map>
#include <unordered_set>

namespace Editor::Assets::HotReload
{
namespace
{
namespace fs = std::filesystem;

// The preflight walks a file with these record sizes; they must be BMD::Open2's.
static_assert(sizeof(Vertex_t) == BMD_VERTEX_BYTES);
static_assert(sizeof(Normal_t) == BMD_NORMAL_BYTES);
static_assert(sizeof(TexCoord_t) == BMD_TEXCOORD_BYTES);
static_assert(sizeof(Triangle_t2) == BMD_TRIANGLE_BYTES);
static_assert(sizeof(vec3_t) == BMD_KEY_BYTES);
static_assert(sizeof(Texture_t::FileName) == BMD_NAME_BYTES);

// Each reload decodes and uploads the model's textures; switching every model of a
// map spreads over some frames instead of stalling one.
constexpr std::size_t MAX_RELOADS_PER_FRAME = 8;
// CGlobalBitmap loads textures up to this size (its MAX_WIDTH and MAX_HEIGHT).
constexpr int MAX_TEXTURE_SIZE = 1024;
// World-object textures load like CMapManager::Load's OpenTexture calls load them.
constexpr GLuint WORLD_TEXTURE_FILTER = GL_NEAREST;
constexpr GLuint WORLD_TEXTURE_WRAP = GL_REPEAT;
constexpr const char* WORLD_OBJECT_WHAT = "world-object";
// Between the reasons a file is refused, in one status line.
constexpr std::string_view PROBLEM_SEPARATOR = "; ";

struct PendingRequest
{
    Request request;
    int worldActive = 0; // the map the request was made on
};

// The model a reload left behind, to tell later whether the engine still shows it.
struct LoadedModel
{
    AssetVariant variant = AssetVariant::Current;
    const Mesh_t* meshes = nullptr;
    std::vector<GLuint> textures; // IndexTexture after the reload
};

// What world code may set on a loaded model and BMD::Open2 resets.
struct ActionTiming
{
    bool loop = false;
    float playSpeed = 0.0f;
};

struct ModelTuning
{
    char streamMesh = -1;
    int boneHead = -1;
    std::vector<ActionTiming> actions;
};

// A texture of the model before the reload.
struct TextureSlot
{
    std::string name;
    GLuint index = BITMAP_UNKNOWN;
};

struct CheckedModel
{
    ModelSummary summary;
    std::vector<TextureFile> textures;
};

std::vector<ModelRange> s_ranges = {WorldObjectRange()};
std::vector<PendingRequest> s_pending;
std::vector<Outcome> s_outcomes;
std::unordered_map<int, LoadedModel> s_loaded;
// Types whose model file passed the check but did not load: the model is empty
// until a later reload succeeds, which stays allowed.
std::unordered_set<int> s_emptied;
// Bitmap index -> file read since the queue was last empty, so a texture that
// several models of one switch share is read once.
std::unordered_map<GLuint, std::wstring> s_readInBatch;

const ModelRange* FindRange(int type)
{
    const auto range = std::find_if(s_ranges.begin(), s_ranges.end(),
                                    [&](const ModelRange& r) { return type >= r.first && type < r.end; });
    return range != s_ranges.end() ? &*range : nullptr;
}

std::string NotLoadedReason(int type)
{
    const ModelRange* range = FindRange(type);
    if (range == nullptr)
        return "type " + std::to_string(type) + " is not a model the editor may reload";
    if (range->followsMap)
        return "this map has no " + range->what + " model of type " + std::to_string(type) + " loaded";
    return "no " + range->what + " model of type " + std::to_string(type) + " is loaded";
}

ModelLimits EngineLimits()
{
    ModelLimits limits;
    limits.maxMeshes = MAX_MESH;
    limits.maxVertices = MAX_VERTICES;
    limits.maxBones = MAX_BONES;
    limits.maxTextureSize = MAX_TEXTURE_SIZE;
    // BITMAP_t::FileName keeps the texture path; BMD::Open2's path buffer (260) and
    // _wsplitpath's folder buffer (_MAX_DIR, 256) take at least as much.
    limits.maxPathBytes = MAX_BITMAP_FILE_NAME - 1;
    return limits;
}

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"[Assets] %hs\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor("[Assets] " + message);
}

bool DecodeModel(std::vector<std::uint8_t>& file, std::vector<std::uint8_t>& model, std::string& error)
{
    BmdEnvelope envelope;
    if (!ReadBmdEnvelope(file, envelope, error))
        return false;
    std::uint8_t* payload = file.data() + envelope.payloadOffset;
    if (!envelope.IsEncrypted())
    {
        model.assign(payload, payload + envelope.payloadSize);
        return true;
    }
    model.resize(envelope.payloadSize); // MapFileDecrypt cannot work in place
    MapFileDecrypt(model.data(), payload, static_cast<int>(envelope.payloadSize));
    return true;
}

bool Preflight(const Request& request, CheckedModel& out, std::string& refusal)
{
    const ModelLimits limits = EngineLimits();
    if (!CanReload(request.type))
    {
        refusal = NotLoadedReason(request.type);
        return false;
    }
    if (!FitsEnginePath(request.bmdFile, limits))
    {
        refusal = Editor::Files::PathToUtf8(request.bmdFile) + " is longer than the client's file-name buffer";
        return false;
    }
    std::vector<std::uint8_t> file = Editor::Files::ReadWholeFile(request.bmdFile);
    if (file.empty())
    {
        refusal = Editor::Files::PathToUtf8(request.bmdFile) + " is missing or empty";
        return false;
    }
    std::vector<std::uint8_t> model;
    if (!DecodeModel(file, model, refusal) ||
        !CheckModelBytes(model.data(), model.size(), limits, out.summary, refusal))
        return false;
    std::vector<std::string> problems;
    out.textures = CheckModelTextures(request.bmdFile.parent_path(), out.summary, limits, problems);
    refusal = Editor::Text::Join(problems, PROBLEM_SEPARATOR);
    return problems.empty();
}

ModelTuning SaveTuning(const BMD& model)
{
    ModelTuning tuning;
    tuning.streamMesh = model.StreamMesh;
    tuning.boneHead = model.BoneHead;
    for (int i = 0; i < model.NumActions; ++i)
        tuning.actions.push_back({model.Actions[i].Loop, model.Actions[i].PlaySpeed});
    return tuning;
}

void RestoreTuning(BMD& model, const ModelTuning& tuning)
{
    model.StreamMesh = tuning.streamMesh;
    model.BoneHead = tuning.boneHead < model.NumBones ? tuning.boneHead : -1;
    const int shared = std::min<int>(model.NumActions, static_cast<int>(tuning.actions.size()));
    for (int i = 0; i < shared; ++i)
    {
        model.Actions[i].Loop = tuning.actions[i].loop;
        model.Actions[i].PlaySpeed = tuning.actions[i].playSpeed;
    }
    model.CurrentAction = 0;
}

// Textures loaded by file name (CGlobalBitmap::LoadImage) get an index from this
// range; the engine's fixed textures below it (BITMAP_HIDE, skins) are never replaced.
bool IsFileTextureIndex(GLuint index)
{
    return index >= BITMAP_NONAMED_TEXTURES_BEGIN && index <= BITMAP_NONAMED_TEXTURES_END;
}

// Texture_t::FileName is a fixed buffer the file fills; it may lack the final NUL.
std::string MeshTextureName(const BMD& model, int mesh)
{
    const char* name = model.Textures[mesh].FileName;
    return std::string(name, std::find(name, name + sizeof(Texture_t::FileName), '\0'));
}

std::vector<TextureSlot> PreviousTextures(const BMD& model)
{
    std::vector<TextureSlot> slots;
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        if (IsFileTextureIndex(model.IndexTexture[i]))
            slots.push_back({MeshTextureName(model, i), model.IndexTexture[i]});
    }
    return slots;
}

bool OpenModelFile(BMD& model, const fs::path& bmdFile)
{
    const std::wstring folder = (bmdFile.parent_path() / "").wstring(); // Open2 appends the name as is
    const std::wstring file = bmdFile.filename().wstring();
    return model.Open2(folder.c_str(), file.c_str(), true);
}

// The index the texture goes to: the old model's texture of that name, else a
// bitmap already holding the file, else none (a new one is made).
std::optional<GLuint> TargetIndex(const std::string& name, const std::wstring& path,
                                  const std::vector<TextureSlot>& previous)
{
    const auto slot = std::find_if(previous.begin(), previous.end(),
                                   [&](const TextureSlot& old) { return SameTextureName(old.name, name); });
    if (slot != previous.end())
        return slot->index;
    if (const BITMAP_t* loaded = Bitmaps.FindTexture(path);
        loaded != nullptr && IsFileTextureIndex(loaded->BitmapIndex))
        return loaded->BitmapIndex;
    return std::nullopt;
}

// Loads one texture without the fatal error of CLoadData::OpenTexture: a file that
// cannot be loaded leaves the mesh on BITMAP_UNKNOWN (drawn white) and a problem.
GLuint LoadTexture(const TextureFile& texture, const fs::path& folder, const ModelRange& range,
                   const std::vector<TextureSlot>& previous, std::vector<std::string>& problems)
{
    if (texture.kind == TextureKind::Hidden)
        return BITMAP_HIDE;
    // The loader swaps in .OZJ/.OZT.
    const std::wstring path = (folder / Editor::Text::Utf8Path(texture.name)).wstring();
    const std::optional<GLuint> target = TargetIndex(texture.name, path, previous);
    GLuint index = BITMAP_UNKNOWN;
    const GLuint filter = range.textureFilter;
    const GLuint wrap = range.textureWrap;
    if (!target)
        index = Bitmaps.LoadImage(path, filter, wrap);
    else if (const auto read = s_readInBatch.find(*target); read != s_readInBatch.end() && read->second == path)
        index = Bitmaps.LoadImage(*target, path, filter, wrap) ? *target : BITMAP_UNKNOWN;
    else
        index = Bitmaps.ReloadImage(*target, path, filter, wrap) ? *target : BITMAP_UNKNOWN;
    if (index == BITMAP_UNKNOWN)
    {
        problems.push_back(texture.name + " could not be loaded and draws white");
        return index;
    }
    s_readInBatch[index] = path;
    return index;
}

void LoadTextures(BMD& model, const std::vector<TextureFile>& textures, const fs::path& folder,
                  const ModelRange& range, const std::vector<TextureSlot>& previous, std::vector<std::string>& problems)
{
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        const std::string name = MeshTextureName(model, i);
        const auto texture = std::find_if(textures.begin(), textures.end(),
                                          [&](const TextureFile& file) { return SameTextureName(file.name, name); });
        model.IndexTexture[i] =
            texture != textures.end() ? LoadTexture(*texture, folder, range, previous, problems) : BITMAP_UNKNOWN;
    }
}

// Placed objects may be in an action or frame the new model does not have; the
// world code reads Actions[CurrentAction] without checking.
void ClampObjectAnimations(int type)
{
    const BMD& model = Models[type];
    for (OBJECT* object : Editor::ObjectPlace::LiveObjectsOfType(type))
    {
        if (object->CurrentAction >= model.NumActions)
            object->CurrentAction = 0;
        if (object->PriorAction >= model.NumActions)
            object->PriorAction = 0;
        const int keys = model.NumActions > 0 ? model.Actions[object->CurrentAction].NumAnimationKeys : 0;
        if (object->AnimationFrame >= keys)
            object->AnimationFrame = 0.0f;
    }
}

void RememberLoad(const Request& request, const BMD& model)
{
    LoadedModel& loaded = s_loaded[request.type];
    loaded.variant = request.variant;
    loaded.meshes = model.Meshs;
    loaded.textures.assign(model.IndexTexture, model.IndexTexture + model.NumMeshs);
}

std::string Describe(const Request& request, const CheckedModel& checked, const std::vector<std::string>& problems)
{
    std::string text = request.modelName + " now shows its " + VariantName(request.variant) +
                       " files: " + Editor::Files::PathToUtf8(request.bmdFile.filename()) + " and " +
                       std::to_string(checked.textures.size()) + " texture(s) from " +
                       Editor::Files::PathToUtf8(request.bmdFile.parent_path()) + ".";
    if (!problems.empty())
        text += " " + Editor::Text::Join(problems, PROBLEM_SEPARATOR) + ".";
    return text;
}

Outcome Reload(const Request& request)
{
    Outcome outcome;
    outcome.request = request;
    CheckedModel checked;
    std::string refusal;
    if (!Preflight(request, checked, refusal))
    {
        outcome.message = request.modelName + " not changed: " + refusal;
        return outcome;
    }
    BMD& model = Models[request.type];
    const ModelTuning tuning = SaveTuning(model);
    const std::vector<TextureSlot> previous = PreviousTextures(model);
    if (!OpenModelFile(model, request.bmdFile))
    {
        s_loaded.erase(request.type);
        s_emptied.insert(request.type);
        outcome.message = request.modelName + ": " + Editor::Files::PathToUtf8(request.bmdFile) +
                          " passed the check but did not load; the model is empty until you switch it again.";
        return outcome;
    }
    s_emptied.erase(request.type);
    RestoreTuning(model, tuning);
    std::vector<std::string> problems;
    LoadTextures(model, checked.textures, request.bmdFile.parent_path(), *FindRange(request.type), previous, problems);
    ClampObjectAnimations(request.type);
    g_ObjectThumbnail.Invalidate(request.type);
    RememberLoad(request, model);
    outcome.loaded = true;
    outcome.message = Describe(request, checked, problems);
    return outcome;
}
} // namespace

ModelRange WorldObjectRange()
{
    ModelRange range;
    range.first = MODEL_WORLD_OBJECT;
    range.end = MAX_WORLD_OBJECTS;
    range.what = WORLD_OBJECT_WHAT;
    range.textureFilter = WORLD_TEXTURE_FILTER;
    range.textureWrap = WORLD_TEXTURE_WRAP;
    range.followsMap = true;
    return range;
}

bool AllowRange(const ModelRange& range)
{
    if (range.first < 0 || range.end <= range.first || range.end > MAX_MODELS)
        return false;
    for (const ModelRange& allowed : s_ranges)
    {
        const bool same = allowed.first == range.first && allowed.end == range.end;
        if (!same && range.first < allowed.end && allowed.first < range.end)
            return false;
    }
    const auto same = std::find_if(s_ranges.begin(), s_ranges.end(),
                                   [&](const ModelRange& r) { return r.first == range.first && r.end == range.end; });
    if (same != s_ranges.end())
        *same = range;
    else
        s_ranges.push_back(range);
    return true;
}

bool CanReload(int type)
{
    if (FindRange(type) == nullptr)
        return false;
    const bool loaded = Models[type].NumMeshs > 0 && Models[type].m_bCompletedAlloc;
    return loaded || s_emptied.contains(type);
}

void Queue(const Request& request)
{
    const auto same = std::find_if(s_pending.begin(), s_pending.end(),
                                   [&](const PendingRequest& pending) { return pending.request.type == request.type; });
    if (same != s_pending.end())
        s_pending.erase(same);
    s_pending.push_back({request, gMapManager.WorldActive});
}

std::size_t PendingCount()
{
    return s_pending.size();
}

void RunPending()
{
    const std::size_t count = std::min(s_pending.size(), MAX_RELOADS_PER_FRAME);
    for (std::size_t i = 0; i < count; ++i)
    {
        const PendingRequest& pending = s_pending[i];
        Outcome outcome;
        outcome.request = pending.request;
        const ModelRange* range = FindRange(pending.request.type);
        const bool mapChanged = pending.worldActive != gMapManager.WorldActive;
        if (mapChanged)
            s_readInBatch.clear(); // the map load replaced the bitmaps it lists
        if (mapChanged && range != nullptr && range->followsMap)
            outcome.message = pending.request.modelName + " not changed: the map changed before the reload ran";
        else
            outcome = Reload(pending.request);
        Log(outcome.message);
        s_outcomes.push_back(std::move(outcome));
    }
    s_pending.erase(s_pending.begin(), s_pending.begin() + static_cast<std::ptrdiff_t>(count));
    if (s_pending.empty())
        s_readInBatch.clear();
}

std::vector<Outcome> TakeOutcomes()
{
    std::vector<Outcome> outcomes;
    outcomes.swap(s_outcomes);
    return outcomes;
}

std::optional<AssetVariant> LoadedVariant(int type)
{
    const auto it = s_loaded.find(type);
    if (it == s_loaded.end() || !CanReload(type))
        return std::nullopt;
    const BMD& model = Models[type];
    const LoadedModel& loaded = it->second;
    const bool sameLoad = model.Meshs == loaded.meshes && model.NumMeshs == (int)loaded.textures.size() &&
                          std::equal(loaded.textures.begin(), loaded.textures.end(), model.IndexTexture);
    if (!sameLoad)
        return std::nullopt;
    return loaded.variant;
}

TextureOrigins CountTexturesFrom(int type, const fs::path& folder)
{
    TextureOrigins origins;
    if (!CanReload(type))
        return origins;
    const std::wstring prefix = (folder / "").wstring();
    const BMD& model = Models[type];
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        if (model.IndexTexture[i] == BITMAP_HIDE)
            continue;
        ++origins.total;
        const BITMAP_t* bitmap = Bitmaps.FindTexture(model.IndexTexture[i]);
        if (bitmap != nullptr && std::wcsncmp(bitmap->FileName, prefix.c_str(), prefix.size()) == 0)
            ++origins.fromFolder;
    }
    return origins;
}
} // namespace Editor::Assets::HotReload

#endif // _EDITOR
