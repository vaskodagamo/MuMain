#include "stdafx.h"

#ifdef _EDITOR

#include "MapEditorFileUtil.h"

#include <string>

namespace fs = std::filesystem;

namespace Editor::Files
{
namespace
{
// Folder names of the client data tree (Data/World7, Data/Object7).
constexpr const wchar_t* WORLD_PREFIX = L"World";
constexpr const wchar_t* OBJECT_PREFIX = L"Object";

// File names inside Data/World{N}. The EncTerrain files carry the map number too
// (EncTerrain7.map); the height and light maps do not.
constexpr const wchar_t* ENC_TERRAIN_PREFIX = L"EncTerrain";
constexpr const wchar_t* MAPPING_EXTENSION = L".map";
constexpr const wchar_t* ATTRIBUTE_EXTENSION = L".att";
constexpr const wchar_t* OBJECT_EXTENSION = L".obj";
constexpr const wchar_t* HEIGHT_FILE = L"TerrainHeight.OZB";
constexpr const wchar_t* LIGHT_FILE = L"TerrainLight.OZJ";

fs::path NumberedFolder(const wchar_t* prefix, int number)
{
    return prefix + std::to_wstring(number);
}

fs::path EncTerrainFile(int world, const wchar_t* extension)
{
    const std::wstring fileName = ENC_TERRAIN_PREFIX + std::to_wstring(world) + extension;
    return WorldDir(world) / fileName;
}
} // namespace

fs::path WorldDir(int world)
{
    return DataDir() / NumberedFolder(WORLD_PREFIX, world);
}

fs::path ObjectDir(int world)
{
    return DataDir() / NumberedFolder(OBJECT_PREFIX, world);
}

fs::path TerrainMappingFile(int world)
{
    return EncTerrainFile(world, MAPPING_EXTENSION);
}

fs::path TerrainAttributeFile(int world)
{
    return EncTerrainFile(world, ATTRIBUTE_EXTENSION);
}

fs::path TerrainObjectFile(int world)
{
    return EncTerrainFile(world, OBJECT_EXTENSION);
}

fs::path TerrainHeightFile(int world)
{
    return WorldDir(world) / HEIGHT_FILE;
}

fs::path TerrainLightFile(int world)
{
    return WorldDir(world) / LIGHT_FILE;
}
} // namespace Editor::Files

#endif // _EDITOR
