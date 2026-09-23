#include "EncTerrainFile.h"

#ifdef _EDITOR

#include "Engine/Object/WorldObjectFile.h"
#include "Render/Terrain/TerrainFiles.h"

namespace Editor::NewMap
{
namespace
{
namespace TerrainFiles = Render::Terrain::Files;

// The .att header: version, folder number, then the map's last tile index twice.
constexpr std::size_t ATTRIBUTE_HEADER_BYTES = 4;
constexpr std::size_t ATTRIBUTE_EXTENT_OFFSET = 2;
constexpr std::uint8_t ATTRIBUTE_EXTENT = 255;
constexpr std::size_t ATTRIBUTE_BYTES = ATTRIBUTE_HEADER_BYTES + TerrainFiles::CELLS;
// The extended layout keeps two bytes a tile; the client reads the low one.
constexpr std::size_t EXTENDED_ATTRIBUTE_BYTES = ATTRIBUTE_HEADER_BYTES + 2 * TerrainFiles::CELLS;

bool CheckAttributeLayout(const Bytes& plain, std::string& error)
{
    if (plain.size() != ATTRIBUTE_BYTES && plain.size() != EXTENDED_ATTRIBUTE_BYTES)
    {
        error = "the walk map is " + std::to_string(plain.size()) + " bytes; the client loads " +
                std::to_string(ATTRIBUTE_BYTES) + " or " + std::to_string(EXTENDED_ATTRIBUTE_BYTES);
        return false;
    }
    if (plain[ATTRIBUTE_EXTENT_OFFSET] != ATTRIBUTE_EXTENT || plain[ATTRIBUTE_EXTENT_OFFSET + 1] != ATTRIBUTE_EXTENT)
    {
        error = "the walk map's header does not describe a 256 x 256 map";
        return false;
    }
    return true;
}

bool CheckObjectsLayout(const Bytes& plain, std::string& error)
{
    Engine::Object::WorldObjectFile::Contents contents;
    if (Engine::Object::WorldObjectFile::Decode(plain.data(), plain.size(), contents))
        return true;
    error = "the object file ends before its last object";
    return false;
}
} // namespace

const char* Extension(EncTerrainKind kind)
{
    switch (kind)
    {
    case EncTerrainKind::Mapping:
        return ".map";
    case EncTerrainKind::Attribute:
        return ".att";
    case EncTerrainKind::Objects:
        return ".obj";
    }
    return "";
}

Bytes DecodeEncTerrain(EncTerrainKind kind, const Bytes& file, const MapFileCodec& codec)
{
    Bytes plain = codec.decrypt(file);
    if (kind == EncTerrainKind::Attribute)
        codec.buxConvert(plain);
    return plain;
}

Bytes EncodeEncTerrain(EncTerrainKind kind, const Bytes& plain, const MapFileCodec& codec)
{
    Bytes stored = plain;
    if (kind == EncTerrainKind::Attribute)
        codec.buxConvert(stored);
    return codec.encrypt(stored);
}

bool CheckPlainLayout(EncTerrainKind kind, const Bytes& plain, std::string& error)
{
    switch (kind)
    {
    case EncTerrainKind::Mapping:
        if (plain.size() >= TerrainFiles::MAPPING_BYTES)
            return true;
        error = "the texture mapping is " + std::to_string(plain.size()) + " bytes; the client needs " +
                std::to_string(TerrainFiles::MAPPING_BYTES);
        return false;
    case EncTerrainKind::Attribute:
        return CheckAttributeLayout(plain, error);
    case EncTerrainKind::Objects:
        return CheckObjectsLayout(plain, error);
    }
    return false;
}

bool Renumber(EncTerrainKind kind, const Bytes& file, int folder, const MapFileCodec& codec, Bytes& out, int& oldFolder,
              std::string& error)
{
    Bytes plain = DecodeEncTerrain(kind, file, codec);
    if (plain.size() <= MAP_NUMBER_OFFSET)
    {
        error = "the file is too short to hold a header";
        return false;
    }
    if (!CheckPlainLayout(kind, plain, error))
        return false;
    oldFolder = plain[MAP_NUMBER_OFFSET];
    plain[MAP_NUMBER_OFFSET] = static_cast<std::uint8_t>(folder);
    out = EncodeEncTerrain(kind, plain, codec);
    return true;
}
} // namespace Editor::NewMap

#endif // _EDITOR
