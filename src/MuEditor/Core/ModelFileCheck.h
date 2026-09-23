#pragma once

#ifdef _EDITOR

#include "ModelHotReload.h"

#include "Assets/ModelPreflight.h"

#include <filesystem>
#include <string>
#include <vector>

class BMD;

// What the hot reload and its side-by-side copies (ModelCopy.h) share: the check
// of a model file and its textures before the engine loads them, and the engine
// calls around that load.
namespace Editor::Assets::HotReload
{
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

ModelTuning SaveTuning(const BMD& model);
// Puts `tuning` on a freshly opened model (for the actions both have).
void RestoreTuning(BMD& model, const ModelTuning& tuning);

// A model file that passed the check, and the texture files it needs.
struct CheckedModel
{
    ModelSummary summary;
    std::vector<TextureFile> textures;
};

// The folders the textures of `request` are looked for in, in order: its
// textureFolders, or the BMD's own folder.
std::vector<std::filesystem::path> TextureFolders(const Request& request);

// Checks the request's BMD and every texture it names against the engine's limits
// (ModelPreflight). False with `refusal` saying why, in one line.
bool CheckFiles(const Request& request, CheckedModel& out, std::string& refusal);

// BMD::Open2 on `bmdFile` (the engine appends the file name to the folder as is).
bool OpenModelFile(BMD& model, const std::filesystem::path& bmdFile);

// The texture name of mesh `mesh` (Texture_t::FileName may lack its final NUL).
std::string MeshTextureName(const BMD& model, int mesh);

// The path the engine's texture loader is given for `texture` (the .jpg or .tga
// name next to its container; the loader reads the .OZJ/.OZT).
std::wstring EngineTexturePath(const TextureFile& texture);

// Marks a loaded texture as skin or hair the way CLoadData::OpenTexture does, so
// the engine hides it with the character's skin (BMD::HideSkin).
void MarkSkinAndHair(unsigned int bitmapIndex, const std::string& textureName);
} // namespace Editor::Assets::HotReload

#endif // _EDITOR
