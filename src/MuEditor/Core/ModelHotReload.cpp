#include "stdafx.h"

#ifdef _EDITOR

#include "ModelHotReload.h"

#include "ModelCopy.h"
#include "ModelFileCheck.h"

#include "Assets/EditorText.h"
#include "Assets/FileDigest.h"
#include "Assets/ModelPreflight.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "Core/EditorFiles.h" // PathToUtf8
#include "UI/MapEditor/MapObjectPlace.h"    // LiveObjectsOfType
#include "UI/MapEditor/ObjectThumbnail.h"

#include "Core/Globals/_enum.h"          // MODEL_WORLD_OBJECT, MAX_WORLD_OBJECTS, MAX_MODELS
#include "Core/Globals/_TextureIndex.h"  // BITMAP_HIDE, BITMAP_UNKNOWN, BITMAP_NONAMED_TEXTURES_*
#include "Engine/Object/w_ObjectInfo.h"  // OBJECT
#include "Render/Models/ZzzBMD.h"        // Models
#include "Render/Sprites/GlobalBitmap.h" // Bitmaps
#include "World/MapInfra/MapManager.h"   // gMapManager.WorldActive

#include <algorithm>
#include <cwchar>
#include <unordered_map>
#include <unordered_set>

namespace Editor::Assets::HotReload
{
namespace
{
namespace fs = std::filesystem;

// Each reload decodes and uploads the model's textures; switching every model of a
// map spreads over some frames instead of stalling one.
constexpr std::size_t MAX_RELOADS_PER_FRAME = 8;
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

// A texture of the model before the reload.
struct TextureSlot
{
    std::string name;
    GLuint index = BITMAP_UNKNOWN;
    bool fixed = false; // one of the engine's own slots (IsFixedTextureIndex)
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

std::string NotLoadedReason(int type)
{
    const ModelRange* range = FindRange(type);
    if (range == nullptr)
        return "type " + std::to_string(type) + " is not a model the editor may reload";
    if (range->followsMap)
        return "this map has no " + range->what + " model of type " + std::to_string(type) + " loaded";
    return "no " + range->what + " model of type " + std::to_string(type) + " is loaded";
}

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"[Assets] %hs\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor("[Assets] " + message);
}

bool Preflight(const Request& request, CheckedModel& out, std::string& refusal)
{
    if (!CanReload(request.type))
    {
        refusal = NotLoadedReason(request.type);
        return false;
    }
    return CheckFiles(request, out, refusal);
}

// Textures loaded by file name (CGlobalBitmap::LoadImage) get an index from this
// range; the engine's fixed textures below it (BITMAP_HIDE, skins) are never replaced.
bool IsFileTextureIndex(GLuint index)
{
    return index >= BITMAP_NONAMED_TEXTURES_BEGIN && index <= BITMAP_NONAMED_TEXTURES_END;
}

// A slot the engine loads a file into by number (LoadBitmap in OpenPlayerTextures and
// OpenItemTextures: hair, capes, event items); other models, effects and the UI use
// the same slot, so a reload never replaces its image.
bool IsFixedTextureIndex(GLuint index)
{
    const bool special = index == BITMAP_HIDE || index == BITMAP_UNKNOWN ||
                         (index >= BITMAP_SKIN_BEGIN && index <= BITMAP_SKIN_END);
    return index < BITMAP_NONAMED_TEXTURES_BEGIN && !special;
}

// The file a fixed slot was read from ("Data\Player\hair_r.jpg" -> Data/Player/hair_r.OZJ).
fs::path FixedSlotFile(const BITMAP_t& bitmap)
{
    std::wstring name = bitmap.FileName;
    std::replace(name.begin(), name.end(), L'\\', L'/');
    const fs::path file(name);
    return FindTextureContainer(file.parent_path(), Editor::Files::PathToUtf8(file.filename()));
}

// The fixed slot the model used for `texture`, when the new file has the same bytes as
// the slot's: the model takes the slot again instead of a second copy of the image.
std::optional<GLuint> SameFixedSlot(const TextureFile& texture, const std::vector<TextureSlot>& previous)
{
    const auto slot = std::find_if(previous.begin(), previous.end(), [&](const TextureSlot& old) {
        return old.fixed && SameTextureName(old.name, texture.name);
    });
    const BITMAP_t* bitmap = slot != previous.end() ? Bitmaps.FindTexture(slot->index) : nullptr;
    if (bitmap == nullptr)
        return std::nullopt;
    const fs::path file = FixedSlotFile(*bitmap);
    const std::string digest = file.empty() ? std::string() : Editor::Files::Sha256Hex(file);
    if (digest.empty() || digest != Editor::Files::Sha256Hex(texture.container))
        return std::nullopt;
    return slot->index;
}

std::vector<TextureSlot> PreviousTextures(const BMD& model)
{
    std::vector<TextureSlot> slots;
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        const GLuint index = model.IndexTexture[i];
        if (IsFileTextureIndex(index) || IsFixedTextureIndex(index))
            slots.push_back({MeshTextureName(model, i), index, IsFixedTextureIndex(index)});
    }
    return slots;
}

// The index the texture goes to: the old model's texture of that name, else a
// bitmap already holding the file (not a side-by-side copy's), else none (a new
// one is made).
std::optional<GLuint> TargetIndex(const std::string& name, const std::wstring& path,
                                  const std::vector<TextureSlot>& previous)
{
    const auto slot = std::find_if(previous.begin(), previous.end(), [&](const TextureSlot& old) {
        return !old.fixed && SameTextureName(old.name, name);
    });
    if (slot != previous.end())
        return slot->index;
    if (const BITMAP_t* loaded = Bitmaps.FindTexture(path);
        loaded != nullptr && IsFileTextureIndex(loaded->BitmapIndex) && !IsCopyTexture(loaded->BitmapIndex))
        return loaded->BitmapIndex;
    return std::nullopt;
}

// Loads one texture without the fatal error of CLoadData::OpenTexture: a file that
// cannot be loaded leaves the mesh on BITMAP_UNKNOWN (drawn white) and a problem.
GLuint LoadTexture(const TextureFile& texture, const ModelRange& range, const std::vector<TextureSlot>& previous,
                   std::vector<std::string>& problems)
{
    if (texture.kind == TextureKind::Hidden)
        return BITMAP_HIDE;
    const GLuint filter = range.textureFilter;
    const GLuint wrap = range.textureWrap;
    if (const std::optional<GLuint> fixed = SameFixedSlot(texture, previous))
    {
        const std::wstring slotFile = Bitmaps.FindTexture(*fixed)->FileName;
        Bitmaps.LoadImage(*fixed, slotFile, filter, wrap); // the same image: one more reference
        return *fixed;
    }
    const std::wstring path = EngineTexturePath(texture);
    const std::optional<GLuint> target = TargetIndex(texture.name, path, previous);
    GLuint index = BITMAP_UNKNOWN;
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
    MarkSkinAndHair(index, texture.name);
    return index;
}

void LoadTextures(BMD& model, const std::vector<TextureFile>& textures, const ModelRange& range,
                  const std::vector<TextureSlot>& previous, std::vector<std::string>& problems)
{
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        const std::string name = MeshTextureName(model, i);
        const auto texture = std::find_if(textures.begin(), textures.end(),
                                          [&](const TextureFile& file) { return SameTextureName(file.name, name); });
        model.IndexTexture[i] =
            texture != textures.end() ? LoadTexture(*texture, range, previous, problems) : BITMAP_UNKNOWN;
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

// The folders the model's textures were read from (a candidate may hold only some).
std::vector<std::string> TextureFolderNames(const Request& request, const CheckedModel& checked)
{
    std::vector<std::string> names;
    for (const TextureFile& texture : checked.textures)
    {
        const std::string folder = Editor::Files::PathToUtf8(texture.container.parent_path());
        if (!texture.container.empty() && std::find(names.begin(), names.end(), folder) == names.end())
            names.push_back(folder);
    }
    if (names.empty())
        names.push_back(Editor::Files::PathToUtf8(request.bmdFile.parent_path()));
    return names;
}

std::string Describe(const Request& request, const CheckedModel& checked, const std::vector<std::string>& problems)
{
    std::string text = request.modelName + " now shows its " + VariantName(request.variant) +
                       " files: " + Editor::Files::PathToUtf8(request.bmdFile.filename()) + " and " +
                       std::to_string(checked.textures.size()) + " texture(s) from " +
                       Editor::Text::Join(TextureFolderNames(request, checked), " and ") + ".";
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
    LoadTextures(model, checked.textures, *FindRange(request.type), previous, problems);
    ClampObjectAnimations(request.type);
    g_ObjectThumbnail.Invalidate(request.type);
    RememberLoad(request, model);
    outcome.loaded = true;
    outcome.message = Describe(request, checked, problems);
    return outcome;
}
} // namespace

const ModelRange* FindRange(int type)
{
    const auto range = std::find_if(s_ranges.begin(), s_ranges.end(),
                                    [&](const ModelRange& r) { return type >= r.first && type < r.end; });
    return range != s_ranges.end() ? &*range : nullptr;
}

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
    ReleaseRetiredCopies();
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

std::vector<Outcome> TakeOutcomes(const ModelRange& range)
{
    std::vector<Outcome> taken;
    std::vector<Outcome> others;
    for (Outcome& outcome : s_outcomes)
    {
        const int type = outcome.request.type;
        (type >= range.first && type < range.end ? taken : others).push_back(std::move(outcome));
    }
    s_outcomes.swap(others);
    return taken;
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
