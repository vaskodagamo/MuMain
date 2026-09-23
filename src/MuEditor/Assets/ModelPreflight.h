#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Checks a model file (.bmd) and the textures it names before the running client
// loads them in place of a model it already shows (the Assets tab's A/B compare).
// The engine's loaders trust their input: BMD::Open2 copies counts and indices from
// the file into fixed-size tables without checking them, and CLoadData::OpenTexture
// ends the game when a texture is missing. Everything they would mis-read, overflow
// or stop on is refused here with a message instead. File system only, no engine
// state, so it is unit-tested on its own (tests/editor/test_model_preflight.cpp).
namespace Editor::Assets
{
// What the engine can hold. The hot reload fills it from ZzzBMD.h (MAX_MESH,
// MAX_VERTICES, MAX_BONES) and GlobalBitmap.h (texture size, file-name buffer).
struct ModelLimits
{
    int maxMeshes = 0;
    int maxVertices = 0; // per mesh; also caps its normals
    int maxBones = 0;
    int maxTextureSize = 0;       // widest and tallest texture, in pixels
    std::size_t maxPathBytes = 0; // longest file path (UTF-8) the engine's fixed name buffers take
};

// A .bmd file is "BMD", a version byte, then the model: plain bytes (0x0A), or an
// int32 size and the model encrypted with the map-file cipher (0x0C).
constexpr std::uint8_t BMD_VERSION_PLAIN = 0x0A;
constexpr std::uint8_t BMD_VERSION_ENCRYPTED = 0x0C;

// On-disk record sizes inside the model, as BMD::Open2 steps over them (the hot
// reload checks them against the engine structs in ZzzBMD.h).
constexpr std::size_t BMD_NAME_BYTES = 32;     // model, texture and bone names
constexpr std::size_t BMD_VERTEX_BYTES = 16;   // Vertex_t
constexpr std::size_t BMD_NORMAL_BYTES = 20;   // Normal_t
constexpr std::size_t BMD_TEXCOORD_BYTES = 8;  // TexCoord_t
constexpr std::size_t BMD_TRIANGLE_BYTES = 64; // Triangle_t2, of which Open2 keeps a Triangle_t
constexpr std::size_t BMD_KEY_BYTES = 12;      // vec3_t, one animation key

struct BmdEnvelope
{
    std::uint8_t version = 0;
    std::size_t payloadOffset = 0; // where the model bytes start in the file
    std::size_t payloadSize = 0;

    bool IsEncrypted() const
    {
        return version == BMD_VERSION_ENCRYPTED;
    }
};

// Reads the header of a .bmd file. False with `error` for anything BMD::Open2
// cannot load (another magic, versions other than 0x0A and 0x0C, a size that does
// not fit the file).
bool ReadBmdEnvelope(const std::vector<std::uint8_t>& file, BmdEnvelope& out, std::string& error);

struct ModelSummary
{
    int meshes = 0;
    int bones = 0;
    int actions = 0;
    std::vector<std::string> meshTextures; // the texture each mesh names, in mesh order
};

// Walks the (decrypted) model bytes the way BMD::Open2 reads them and checks that
// every record lies inside the data, every count fits `limits` and every index
// the renderer follows (triangle corners, bone parents) points at an existing
// entry. Each mesh must use its own texture slot, as every shipped model does:
// BMD::Release drops one texture reference per mesh through that slot. False with
// `error` naming the first problem.
bool CheckModelBytes(const std::uint8_t* data, std::size_t size, const ModelLimits& limits, ModelSummary& out,
                     std::string& error);

// How CLoadData::OpenTexture treats a texture name: "hid..." marks a hidden mesh
// (no file), *.jpg loads <name>.OZJ, *.tga loads <name>.OZT, anything else cannot load.
enum class TextureKind
{
    Hidden,
    Jpeg,
    Tga,
    Unsupported,
};
TextureKind TextureKindOf(const std::string& textureName);

// True when two texture names load the same file: the client compares them
// without regard to (ASCII) case.
bool SameTextureName(const std::string& a, const std::string& b);

// The file `folder` holds for `textureName` (tree.jpg -> tree.OZJ), matched without
// regard to case as the client does on every platform; empty when there is none.
std::filesystem::path FindTextureContainer(const std::filesystem::path& folder, const std::string& textureName);

// A texture the model needs and the file that holds it.
struct TextureFile
{
    std::string name; // as the model names it, e.g. tree.jpg
    TextureKind kind = TextureKind::Hidden;
    std::filesystem::path container; // e.g. <folder>/tree.OZJ; empty for a hidden mesh
    int width = 0;
    int height = 0;
};

// Checks one .OZJ (24-byte prefix + JPEG) or .OZT (4-byte prefix + uncompressed
// 32-bit TGA) the way CGlobalBitmap reads it, and fills in its size.
bool CheckTextureContainer(const std::filesystem::path& file, TextureKind kind, int maxSize, TextureFile& inOut,
                           std::string& error);

// Every distinct texture of `model`, found in `folder` and checked. Each problem is
// appended to `problems`; the textures come back in first-use order.
std::vector<TextureFile> CheckModelTextures(const std::filesystem::path& folder, const ModelSummary& model,
                                            const ModelLimits& limits, std::vector<std::string>& problems);

// True when the engine's fixed file-name buffers can hold `path`.
bool FitsEnginePath(const std::filesystem::path& path, const ModelLimits& limits);
} // namespace Editor::Assets

#endif // _EDITOR
