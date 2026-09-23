#include "ModelPreflight.h"

#ifdef _EDITOR

#include "EditorText.h"

#include "turbojpeg.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>

namespace Editor::Assets
{
using Editor::Text::EqualIgnoringCase;
using Editor::Text::Join;
using Editor::Text::PathToUtf8;
using Editor::Text::Utf8Path;

namespace
{
constexpr char BMD_MAGIC[] = {'B', 'M', 'D'};
constexpr std::size_t BMD_MAGIC_BYTES = sizeof(BMD_MAGIC);
constexpr std::size_t BMD_VERSION_OFFSET = BMD_MAGIC_BYTES;
constexpr std::size_t BMD_PLAIN_OFFSET = BMD_VERSION_OFFSET + 1;
constexpr std::size_t BMD_SIZE_BYTES = sizeof(std::int32_t);
constexpr std::size_t BMD_ENCRYPTED_OFFSET = BMD_PLAIN_OFFSET + BMD_SIZE_BYTES;

// Triangle_t inside each triangle record: a polygon corner count (3 or 4), then
// four vertex, four normal and four texture-coordinate indices.
constexpr std::size_t TRIANGLE_VERTEX_INDEX_OFFSET = 2;
constexpr std::size_t TRIANGLE_NORMAL_INDEX_OFFSET = 10;
constexpr std::size_t TRIANGLE_TEXCOORD_INDEX_OFFSET = 18;
constexpr int TRIANGLE_CORNERS = 3;
constexpr int QUAD_CORNERS = 4;
constexpr int NO_PARENT_BONE = -1;

// .OZJ: 24 bytes the loader skips, then a JPEG. .OZT: 4 bytes, then a TGA whose
// 18-byte header the loader reads at fixed places; the pixels follow directly.
constexpr std::size_t OZJ_PREFIX_BYTES = 24;
constexpr std::size_t OZT_PREFIX_BYTES = 4;
constexpr std::size_t TGA_HEADER_BYTES = 18;
constexpr std::size_t TGA_ID_LENGTH_OFFSET = 0;
constexpr std::size_t TGA_IMAGE_TYPE_OFFSET = 2;
constexpr std::size_t TGA_WIDTH_OFFSET = 12;
constexpr std::size_t TGA_HEIGHT_OFFSET = 14;
constexpr std::size_t TGA_BITS_OFFSET = 16;
constexpr std::uint8_t TGA_UNCOMPRESSED_TRUE_COLOR = 2;
constexpr std::uint8_t TGA_BITS_PER_PIXEL = 32;
constexpr std::size_t TGA_BYTES_PER_PIXEL = 4;

constexpr const char* HIDDEN_TEXTURE_PREFIX = "hid";
constexpr const char* JPEG_EXTENSION = ".jpg";
constexpr const char* TGA_EXTENSION = ".tga";
constexpr const char* JPEG_CONTAINER_EXTENSION = ".OZJ";
constexpr const char* TGA_CONTAINER_EXTENSION = ".OZT";

// Reads the model bytes front to back; every read fails instead of passing the end.
class ModelReader
{
public:
    ModelReader(const std::uint8_t* data, std::size_t size) : m_data(data), m_size(size) {}

    bool Skip(std::size_t bytes)
    {
        if (bytes > m_size - m_position)
            return false;
        m_position += bytes;
        return true;
    }

    bool ReadInt16(int& out)
    {
        std::int16_t value = 0;
        if (!Peek(&value, sizeof(value)))
            return false;
        out = value;
        return Skip(sizeof(value));
    }

    bool ReadByte(std::uint8_t& out)
    {
        return Peek(&out, sizeof(out)) && Skip(sizeof(out));
    }

    bool ReadName(std::string& out)
    {
        char name[BMD_NAME_BYTES] = {};
        if (!Peek(name, sizeof(name)))
            return false;
        out.assign(name, std::find(name, name + sizeof(name), '\0'));
        return Skip(sizeof(name));
    }

    // Copies `bytes` from `offset` past the current position without moving.
    bool Peek(void* out, std::size_t bytes, std::size_t offset = 0) const
    {
        if (offset > m_size - m_position || bytes > m_size - m_position - offset)
            return false;
        std::memcpy(out, m_data + m_position + offset, bytes);
        return true;
    }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_position = 0;
};

struct MeshCounts
{
    int vertices = 0;
    int normals = 0;
    int texCoords = 0;
    int triangles = 0;
    int textureSlot = 0;
};

std::string MeshText(int mesh)
{
    return "mesh " + std::to_string(mesh);
}

bool ReadModelHeader(ModelReader& reader, const ModelLimits& limits, ModelSummary& summary, std::string& error)
{
    if (!reader.Skip(BMD_NAME_BYTES) || !reader.ReadInt16(summary.meshes) || !reader.ReadInt16(summary.bones) ||
        !reader.ReadInt16(summary.actions))
    {
        error = "the model ends inside its header";
        return false;
    }
    if (summary.meshes <= 0 || summary.meshes > limits.maxMeshes)
        error = std::to_string(summary.meshes) + " meshes; the engine takes 1 to " + std::to_string(limits.maxMeshes);
    else if (summary.bones < 0 || summary.bones > limits.maxBones)
        error = std::to_string(summary.bones) + " bones; the engine takes at most " + std::to_string(limits.maxBones);
    else if (summary.actions < 0)
        error = "a negative action count";
    return error.empty();
}

bool ReadMeshCounts(ModelReader& reader, int mesh, const ModelSummary& summary, const ModelLimits& limits,
                    MeshCounts& counts, std::string& error)
{
    if (!reader.ReadInt16(counts.vertices) || !reader.ReadInt16(counts.normals) ||
        !reader.ReadInt16(counts.texCoords) || !reader.ReadInt16(counts.triangles) ||
        !reader.ReadInt16(counts.textureSlot))
    {
        error = "the model ends inside " + MeshText(mesh);
        return false;
    }
    if (counts.vertices < 0 || counts.normals < 0 || counts.texCoords < 0 || counts.triangles < 0)
        error = MeshText(mesh) + " has a negative count";
    else if (counts.vertices > limits.maxVertices || counts.normals > limits.maxVertices)
        error = MeshText(mesh) + " has " + std::to_string(std::max(counts.vertices, counts.normals)) +
                " vertices or normals; the engine takes at most " + std::to_string(limits.maxVertices);
    else if (counts.textureSlot != mesh)
        error = MeshText(mesh) + " uses texture slot " + std::to_string(counts.textureSlot) +
                "; the client counts texture references per mesh, so each mesh must use its own slot";
    return error.empty();
}

bool IndexInRange(std::int16_t index, int count)
{
    return index >= 0 && index < count;
}

bool CheckTriangle(const ModelReader& reader, const MeshCounts& counts, int triangle, int mesh, std::string& error)
{
    std::uint8_t corners = 0;
    std::int16_t vertex[QUAD_CORNERS] = {};
    std::int16_t normal[QUAD_CORNERS] = {};
    std::int16_t texCoord[QUAD_CORNERS] = {};
    if (!reader.Peek(&corners, sizeof(corners)) || !reader.Peek(vertex, sizeof(vertex), TRIANGLE_VERTEX_INDEX_OFFSET) ||
        !reader.Peek(normal, sizeof(normal), TRIANGLE_NORMAL_INDEX_OFFSET) ||
        !reader.Peek(texCoord, sizeof(texCoord), TRIANGLE_TEXCOORD_INDEX_OFFSET))
    {
        error = "the model ends inside " + MeshText(mesh);
        return false;
    }
    const std::string where = MeshText(mesh) + ", triangle " + std::to_string(triangle);
    if (corners != TRIANGLE_CORNERS && corners != QUAD_CORNERS)
    {
        error = where + " has " + std::to_string(corners) + " corners";
        return false;
    }
    for (int corner = 0; corner < corners; ++corner)
    {
        if (!IndexInRange(vertex[corner], counts.vertices) || !IndexInRange(normal[corner], counts.normals) ||
            !IndexInRange(texCoord[corner], counts.texCoords))
        {
            error = where + " points past the mesh's vertices, normals or texture coordinates";
            return false;
        }
    }
    return true;
}

bool ReadMesh(ModelReader& reader, int mesh, const ModelLimits& limits, ModelSummary& summary, std::string& error)
{
    const std::string cutShort = "the model ends inside " + MeshText(mesh);
    MeshCounts counts;
    if (!ReadMeshCounts(reader, mesh, summary, limits, counts, error))
        return false;
    const std::size_t arrays =
        counts.vertices * BMD_VERTEX_BYTES + counts.normals * BMD_NORMAL_BYTES + counts.texCoords * BMD_TEXCOORD_BYTES;
    if (!reader.Skip(arrays))
    {
        error = cutShort;
        return false;
    }
    for (int triangle = 0; triangle < counts.triangles; ++triangle)
    {
        if (!CheckTriangle(reader, counts, triangle, mesh, error))
            return false;
        if (!reader.Skip(BMD_TRIANGLE_BYTES))
        {
            error = cutShort;
            return false;
        }
    }
    std::string texture;
    if (!reader.ReadName(texture))
    {
        error = cutShort;
        return false;
    }
    summary.meshTextures.push_back(texture);
    return true;
}

bool ReadActions(ModelReader& reader, int actions, std::vector<int>& keys, std::string& error)
{
    for (int action = 0; action < actions; ++action)
    {
        int keyCount = 0;
        std::uint8_t lockPositions = 0;
        if (!reader.ReadInt16(keyCount) || !reader.ReadByte(lockPositions))
        {
            error = "the model ends inside action " + std::to_string(action);
            return false;
        }
        if (keyCount < 1)
        {
            error = "action " + std::to_string(action) + " has no animation keys";
            return false;
        }
        if (lockPositions != 0 && !reader.Skip(keyCount * BMD_KEY_BYTES))
        {
            error = "the model ends inside action " + std::to_string(action);
            return false;
        }
        keys.push_back(keyCount);
    }
    return true;
}

// A bone record: a dummy flag; a real bone adds its name, its parent and, for each
// action, a position and a rotation per key.
bool ReadBone(ModelReader& reader, int bone, int bones, const std::vector<int>& keys, std::string& error)
{
    const std::string cutShort = "the model ends inside bone " + std::to_string(bone);
    std::uint8_t dummy = 0;
    std::string name;
    int parent = NO_PARENT_BONE;
    if (!reader.ReadByte(dummy))
    {
        error = cutShort;
        return false;
    }
    if (dummy != 0)
        return true;
    if (!reader.ReadName(name) || !reader.ReadInt16(parent))
    {
        error = cutShort;
        return false;
    }
    if (parent < NO_PARENT_BONE || parent >= bones)
    {
        error =
            "bone " + std::to_string(bone) + " has parent " + std::to_string(parent) + " of " + std::to_string(bones);
        return false;
    }
    for (int keyCount : keys)
    {
        if (!reader.Skip(2 * keyCount * BMD_KEY_BYTES))
        {
            error = cutShort;
            return false;
        }
    }
    return true;
}

bool ReadBones(ModelReader& reader, int bones, const std::vector<int>& keys, std::string& error)
{
    for (int bone = 0; bone < bones; ++bone)
    {
        if (!ReadBone(reader, bone, bones, keys, error))
            return false;
    }
    return true;
}

bool EndsWithIgnoringCase(const std::string& text, const std::string& suffix)
{
    return text.size() >= suffix.size() && EqualIgnoringCase(text.substr(text.size() - suffix.size()), suffix);
}

std::string ContainerName(const std::string& textureName, TextureKind kind)
{
    const std::size_t dot = textureName.rfind('.');
    const std::string stem = textureName.substr(0, dot);
    return stem + (kind == TextureKind::Jpeg ? JPEG_CONTAINER_EXTENSION : TGA_CONTAINER_EXTENSION);
}

std::vector<std::uint8_t> ReadFileBytes(const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

bool SizeFits(int width, int height, int maxSize)
{
    return width > 0 && height > 0 && width <= maxSize && height <= maxSize;
}

std::string SizeText(int width, int height)
{
    return std::to_string(width) + "x" + std::to_string(height);
}

bool CheckJpegContainer(const std::vector<std::uint8_t>& bytes, int maxSize, TextureFile& inOut, std::string& error)
{
    if (bytes.size() <= OZJ_PREFIX_BYTES)
    {
        error = "too short for an .OZJ";
        return false;
    }
    tjhandle decoder = tjInitDecompress();
    if (decoder == nullptr)
    {
        error = "the JPEG decoder did not start";
        return false;
    }
    int subsampling = 0;
    int colorspace = 0;
    const int result = tjDecompressHeader3(decoder, bytes.data() + OZJ_PREFIX_BYTES,
                                           static_cast<unsigned long>(bytes.size() - OZJ_PREFIX_BYTES), &inOut.width,
                                           &inOut.height, &subsampling, &colorspace);
    tjDestroy(decoder);
    if (result != 0)
        error = "not a readable JPEG after the 24-byte .OZJ prefix";
    else if (!SizeFits(inOut.width, inOut.height, maxSize))
        error = SizeText(inOut.width, inOut.height) + " px; the client loads at most " + std::to_string(maxSize);
    return error.empty();
}

int ReadUint16Le(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return bytes[offset] | (bytes[offset + 1] << 8);
}

bool CheckTgaContainer(const std::vector<std::uint8_t>& bytes, int maxSize, TextureFile& inOut, std::string& error)
{
    if (bytes.size() < OZT_PREFIX_BYTES + TGA_HEADER_BYTES)
    {
        error = "too short for an .OZT";
        return false;
    }
    const std::size_t header = OZT_PREFIX_BYTES;
    inOut.width = ReadUint16Le(bytes, header + TGA_WIDTH_OFFSET);
    inOut.height = ReadUint16Le(bytes, header + TGA_HEIGHT_OFFSET);
    const std::size_t pixelBytes = static_cast<std::size_t>(inOut.width) * inOut.height * TGA_BYTES_PER_PIXEL;
    if (bytes[header + TGA_ID_LENGTH_OFFSET] != 0 ||
        bytes[header + TGA_IMAGE_TYPE_OFFSET] != TGA_UNCOMPRESSED_TRUE_COLOR)
        error = "not an uncompressed TGA without an image ID (the client reads only those)";
    else if (bytes[header + TGA_BITS_OFFSET] != TGA_BITS_PER_PIXEL)
        error = std::to_string(bytes[header + TGA_BITS_OFFSET]) + "-bit; the client reads 32-bit TGAs";
    else if (!SizeFits(inOut.width, inOut.height, maxSize))
        error = SizeText(inOut.width, inOut.height) + " px; the client loads at most " + std::to_string(maxSize);
    else if (bytes.size() < header + TGA_HEADER_BYTES + pixelBytes)
        error = "shorter than its " + SizeText(inOut.width, inOut.height) + " pixels";
    return error.empty();
}

// The first of `folders` that holds the texture's file; empty when none does.
std::filesystem::path FindInFolders(const std::vector<std::filesystem::path>& folders, const std::string& name)
{
    for (const std::filesystem::path& folder : folders)
    {
        std::filesystem::path container = FindTextureContainer(folder, name);
        if (!container.empty())
            return container;
    }
    return {};
}

std::string FolderList(const std::vector<std::filesystem::path>& folders)
{
    std::vector<std::string> names;
    for (const std::filesystem::path& folder : folders)
        names.push_back(PathToUtf8(folder));
    return Join(names, " or ");
}

void CheckOneTexture(const std::vector<std::filesystem::path>& folders, const ModelLimits& limits,
                     TextureFile& texture, std::vector<std::string>& problems)
{
    const std::string label = "texture " + texture.name + ": ";
    if (texture.kind == TextureKind::Unsupported)
    {
        problems.push_back(label + "the client loads only .jpg and .tga textures");
        return;
    }
    texture.container = FindInFolders(folders, texture.name);
    if (texture.container.empty())
    {
        problems.push_back(label + ContainerName(texture.name, texture.kind) + " is missing in " + FolderList(folders));
        return;
    }
    if (!FitsEnginePath(texture.container.parent_path() / Utf8Path(texture.name), limits))
    {
        problems.push_back(label + "its path is longer than the client's file-name buffer");
        return;
    }
    std::string error;
    if (!CheckTextureContainer(texture.container, texture.kind, limits.maxTextureSize, texture, error))
        problems.push_back(label + PathToUtf8(texture.container.filename()) + " is " + error);
}
} // namespace

bool ReadBmdEnvelope(const std::vector<std::uint8_t>& file, BmdEnvelope& out, std::string& error)
{
    if (file.size() < BMD_PLAIN_OFFSET || std::memcmp(file.data(), BMD_MAGIC, BMD_MAGIC_BYTES) != 0)
    {
        error = "not a BMD file";
        return false;
    }
    out.version = file[BMD_VERSION_OFFSET];
    if (out.version == BMD_VERSION_PLAIN)
    {
        out.payloadOffset = BMD_PLAIN_OFFSET;
        out.payloadSize = file.size() - BMD_PLAIN_OFFSET;
        return true;
    }
    if (out.version != BMD_VERSION_ENCRYPTED)
    {
        error = "BMD version " + std::to_string(out.version) + "; the client reads versions 10 and 12";
        return false;
    }
    std::int32_t size = 0;
    if (file.size() >= BMD_ENCRYPTED_OFFSET)
        std::memcpy(&size, file.data() + BMD_PLAIN_OFFSET, BMD_SIZE_BYTES);
    if (file.size() < BMD_ENCRYPTED_OFFSET || size < 0 ||
        static_cast<std::size_t>(size) > file.size() - BMD_ENCRYPTED_OFFSET)
    {
        error = "the encrypted model size does not fit the file";
        return false;
    }
    out.payloadOffset = BMD_ENCRYPTED_OFFSET;
    out.payloadSize = static_cast<std::size_t>(size);
    return true;
}

bool CheckModelBytes(const std::uint8_t* data, std::size_t size, const ModelLimits& limits, ModelSummary& out,
                     std::string& error)
{
    ModelReader reader(data, size);
    ModelSummary summary;
    if (!ReadModelHeader(reader, limits, summary, error))
        return false;
    for (int mesh = 0; mesh < summary.meshes; ++mesh)
    {
        if (!ReadMesh(reader, mesh, limits, summary, error))
            return false;
    }
    std::vector<int> keys;
    if (!ReadActions(reader, summary.actions, keys, error) || !ReadBones(reader, summary.bones, keys, error))
        return false;
    out = std::move(summary);
    return true;
}

TextureKind TextureKindOf(const std::string& textureName)
{
    const std::string hiddenPrefix = HIDDEN_TEXTURE_PREFIX;
    if (EqualIgnoringCase(textureName.substr(0, hiddenPrefix.size()), hiddenPrefix))
        return TextureKind::Hidden;
    if (EndsWithIgnoringCase(textureName, JPEG_EXTENSION))
        return TextureKind::Jpeg;
    if (EndsWithIgnoringCase(textureName, TGA_EXTENSION))
        return TextureKind::Tga;
    return TextureKind::Unsupported;
}

bool SameTextureName(const std::string& a, const std::string& b)
{
    return EqualIgnoringCase(a, b);
}

std::filesystem::path FindTextureContainer(const std::filesystem::path& folder, const std::string& textureName)
{
    const TextureKind kind = TextureKindOf(textureName);
    if (kind != TextureKind::Jpeg && kind != TextureKind::Tga)
        return {};
    const std::string wanted = ContainerName(textureName, kind);
    std::error_code ec;
    const std::filesystem::path exact = folder / Utf8Path(wanted);
    if (std::filesystem::is_regular_file(exact, ec))
        return exact;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec))
    {
        if (entry.is_regular_file(ec) && EqualIgnoringCase(PathToUtf8(entry.path().filename()), wanted))
            return entry.path();
    }
    return {};
}

bool CheckTextureContainer(const std::filesystem::path& file, TextureKind kind, int maxSize, TextureFile& inOut,
                           std::string& error)
{
    const std::vector<std::uint8_t> bytes = ReadFileBytes(file);
    if (bytes.empty())
    {
        error = "empty or unreadable";
        return false;
    }
    if (kind == TextureKind::Jpeg)
        return CheckJpegContainer(bytes, maxSize, inOut, error);
    if (kind == TextureKind::Tga)
        return CheckTgaContainer(bytes, maxSize, inOut, error);
    error = "not a texture the client loads";
    return false;
}

std::vector<TextureFile> CheckModelTextures(const std::filesystem::path& folder, const ModelSummary& model,
                                            const ModelLimits& limits, std::vector<std::string>& problems)
{
    return CheckModelTextures(std::vector<std::filesystem::path>{folder}, model, limits, problems);
}

std::vector<TextureFile> CheckModelTextures(const std::vector<std::filesystem::path>& folders,
                                            const ModelSummary& model, const ModelLimits& limits,
                                            std::vector<std::string>& problems)
{
    std::vector<TextureFile> textures;
    for (const std::string& name : model.meshTextures)
    {
        const bool seen = std::any_of(textures.begin(), textures.end(),
                                      [&](const TextureFile& texture) { return SameTextureName(texture.name, name); });
        if (seen)
            continue;
        TextureFile texture;
        texture.name = name;
        texture.kind = TextureKindOf(name);
        if (texture.kind != TextureKind::Hidden)
            CheckOneTexture(folders, limits, texture, problems);
        textures.push_back(texture);
    }
    return textures;
}

bool FitsEnginePath(const std::filesystem::path& path, const ModelLimits& limits)
{
    return path.u8string().size() <= limits.maxPathBytes;
}
} // namespace Editor::Assets

#endif // _EDITOR
