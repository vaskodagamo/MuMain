#include "stdafx.h"

#ifdef _EDITOR

#include "ModelFileCheck.h"

#include "Assets/EditorText.h"
#include "Core/EditorFiles.h" // ReadWholeFile, PathToUtf8

#include "Render/Models/ZzzBMD.h"         // MAX_MESH, MAX_VERTICES, MAX_BONES
#include "Render/Sprites/GlobalBitmap.h"  // Bitmaps, MAX_BITMAP_FILE_NAME
#include "Render/Terrain/ZzzLodTerrain.h" // MapFileDecrypt

#include <algorithm>

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

// CGlobalBitmap loads textures up to this size (its MAX_WIDTH and MAX_HEIGHT).
constexpr int MAX_TEXTURE_SIZE = 1024;
// Between the reasons a file is refused, in one status line.
constexpr std::string_view PROBLEM_SEPARATOR = "; ";
// CLoadData::OpenTexture's prefixes for skin and hair textures (the first two
// compared as written, "level" in any case).
constexpr std::string_view SKIN_PREFIX = "ski";
constexpr std::string_view LEVEL_SKIN_PREFIX = "level";
constexpr std::string_view HAIR_PREFIX = "hair";

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

bool StartsWith(const std::string& text, std::string_view prefix)
{
    return text.size() >= prefix.size() && std::string_view(text).substr(0, prefix.size()) == prefix;
}
} // namespace

std::vector<fs::path> TextureFolders(const Request& request)
{
    if (!request.textureFolders.empty())
        return request.textureFolders;
    return {request.bmdFile.parent_path()};
}

bool CheckFiles(const Request& request, CheckedModel& out, std::string& refusal)
{
    const ModelLimits limits = EngineLimits();
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
    out.textures = CheckModelTextures(TextureFolders(request), out.summary, limits, problems);
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

bool OpenModelFile(BMD& model, const fs::path& bmdFile)
{
    const std::wstring folder = (bmdFile.parent_path() / "").wstring(); // Open2 appends the name as is
    const std::wstring file = bmdFile.filename().wstring();
    return model.Open2(folder.c_str(), file.c_str(), true);
}

std::string MeshTextureName(const BMD& model, int mesh)
{
    const char* name = model.Textures[mesh].FileName;
    return std::string(name, std::find(name, name + sizeof(Texture_t::FileName), '\0'));
}

std::wstring EngineTexturePath(const TextureFile& texture)
{
    return (texture.container.parent_path() / Editor::Text::Utf8Path(texture.name)).wstring();
}

void MarkSkinAndHair(unsigned int bitmapIndex, const std::string& textureName)
{
    const bool isSkin = StartsWith(textureName, SKIN_PREFIX) ||
                        Editor::Text::EqualIgnoringCase(textureName.substr(0, LEVEL_SKIN_PREFIX.size()),
                                                        LEVEL_SKIN_PREFIX);
    const bool isHair = StartsWith(textureName, HAIR_PREFIX);
    if (!isSkin && !isHair)
        return;
    if (BITMAP_t* bitmap = Bitmaps.FindTexture(bitmapIndex))
    {
        bitmap->IsSkin = isSkin;
        bitmap->IsHair = isHair;
    }
}
} // namespace Editor::Assets::HotReload

#endif // _EDITOR
