#include "stdafx.h"

#ifdef _EDITOR

#include "LiveMapFiles.h"

#include "Assets/TerrainHeightFile.h"
#include "Assets/TerrainLightFile.h"
#include "LiveMap.h"
#include "MapInspect/TileArea.h" // WholeMap
#include "MapScript/AttributeRules.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "UI/MapEditor/MapAttributeSave.h"
#include "UI/MapEditor/MapEditHistory.h"
#include "UI/MapEditor/MapEditorSave.h"
#include "UI/MapEditor/MapEditorUI.h"
#include "UI/MapEditor/MapHeightSave.h"
#include "UI/MapEditor/MapLightSave.h"
#include "UI/MapEditor/MapObjectPlace.h"
#include "UI/MapEditor/MapTerrainLayers.h"

#include "Core/Globals/_crypt.h" // BuxConvert
#include "Engine/Object/WorldObjectFile.h"
#include "Engine/Object/ZzzObject.h" // OpenObjectsEnc, IsSavedWorldObject
#include "Render/Terrain/TerrainFiles.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "World/MapInfra/MapManager.h"

#include <cstring>

namespace Editor::LiveMapFiles
{
namespace
{
constexpr std::size_t CELLS = Editor::MapInspect::MAP_CELLS;
constexpr int LIGHT_CHANNELS = 3;

// The files of the units a revert reads, decoded before anything changes.
struct DecodedFiles
{
    std::vector<std::uint8_t> baseTiles;
    std::vector<std::uint8_t> overlayTiles;
    std::vector<float> overlayAlpha;
    std::vector<float> heights;
    std::vector<std::uint16_t> walls;
    std::vector<float> light;
    std::filesystem::path objectFile;
};

std::string Describe(const std::filesystem::path& file)
{
    return Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(file));
}

// The file's bytes after MapFileDecrypt; empty (with the reason) when it cannot be read.
std::vector<std::uint8_t> ReadDecrypted(const std::filesystem::path& file, std::string& error)
{
    std::vector<std::uint8_t> encrypted = Editor::Files::ReadWholeFile(file);
    if (encrypted.empty())
    {
        error = "cannot read " + Describe(file);
        return {};
    }
    std::vector<std::uint8_t> plain(encrypted.size());
    MapFileDecrypt(plain.data(), encrypted.data(), static_cast<int>(encrypted.size()));
    return plain;
}

bool DecodeTexture(int world, DecodedFiles& files, std::string& error)
{
    const std::filesystem::path file = Editor::Files::TerrainMappingFile(world);
    const std::vector<std::uint8_t> plain = ReadDecrypted(file, error);
    if (plain.empty())
        return false;
    files.baseTiles.resize(CELLS);
    files.overlayTiles.resize(CELLS);
    files.overlayAlpha.resize(CELLS);
    const Render::Terrain::Files::MappingLayers layers{files.baseTiles.data(), files.overlayTiles.data(),
                                                       files.overlayAlpha.data()};
    if (Render::Terrain::Files::DecodeMapping(plain.data(), plain.size(), layers, error) >= 0)
        return true;
    error = Describe(file) + ": " + error;
    return false;
}

bool DecodeHeights(int world, DecodedFiles& files, std::string& error)
{
    const std::filesystem::path file = Editor::Files::TerrainHeightFile(world);
    const std::vector<std::uint8_t> bytes = Editor::Files::ReadWholeFile(file);
    files.heights.resize(CELLS);
    const bool decoded =
        IsTerrainHeightExtMap(gMapManager.WorldActive)
            ? Render::Terrain::Files::DecodeExtendedHeight(bytes.data(), bytes.size(), g_fMinHeight,
                                                           files.heights.data(), error)
            : Editor::HeightMap::DecodeOzb(bytes, Editor::LiveMap::HeightFactor(), files.heights.data(), error);
    if (!decoded)
        error = Describe(file) + ": " + error;
    return decoded;
}

bool DecodeWalls(int world, DecodedFiles& files, std::string& error)
{
    const std::filesystem::path file = Editor::Files::TerrainAttributeFile(world);
    std::vector<std::uint8_t> plain = ReadDecrypted(file, error);
    if (plain.empty())
        return false;
    BuxConvert(plain.data(), static_cast<int>(plain.size()));
    if (Editor::MapScript::Attributes::DecodeClientFile(plain.data(), plain.size(), gMapManager.WorldActive,
                                                        files.walls, error))
        return true;
    error = Describe(file) + ": " + error;
    return false;
}

bool DecodeLight(int world, DecodedFiles& files, std::string& error)
{
    const std::filesystem::path file = Editor::Files::TerrainLightFile(world);
    files.light.resize(CELLS * LIGHT_CHANNELS);
    if (Editor::LightMap::DecodeOzj(Editor::Files::ReadWholeFile(file), TERRAIN_SIZE, files.light.data(), error))
        return true;
    error = Describe(file) + ": " + error;
    return false;
}

bool CheckObjects(int world, DecodedFiles& files, std::string& error)
{
    const std::filesystem::path file = Editor::Files::TerrainObjectFile(world);
    const std::vector<std::uint8_t> plain = ReadDecrypted(file, error);
    if (plain.empty())
        return false;
    Engine::Object::WorldObjectFile::Contents contents;
    if (!Engine::Object::WorldObjectFile::Decode(plain.data(), plain.size(), contents))
    {
        error = Describe(file) + ": the object list is cut short";
        return false;
    }
    files.objectFile = file;
    return true;
}

bool Decode(SaveUnit unit, int world, DecodedFiles& files, std::string& error)
{
    switch (unit)
    {
    case SaveUnit::Texture:
        return DecodeTexture(world, files, error);
    case SaveUnit::Height:
        return DecodeHeights(world, files, error);
    case SaveUnit::Attribute:
        return DecodeWalls(world, files, error);
    case SaveUnit::Light:
        return DecodeLight(world, files, error);
    case SaveUnit::Objects:
        return CheckObjects(world, files, error);
    }
    return false;
}

// The saved objects are freed and the file's records created again, each with its
// record index as its place in the next save (as a map load does).
void ReloadObjects(const std::filesystem::path& file)
{
    std::vector<OBJECT*> saved;
    Editor::ObjectPlace::ForEachLiveObject(
        [&saved](OBJECT* object)
        {
            if (IsSavedWorldObject(object))
                saved.push_back(object);
        });
    for (OBJECT* object : saved)
        Editor::ObjectPlace::Remove(object);
    std::wstring name = file.wstring(); // OpenObjectsEnc takes a mutable buffer
    OpenObjectsEnc(name.data());
}

void Restore(SaveUnit unit, const DecodedFiles& files)
{
    switch (unit)
    {
    case SaveUnit::Texture:
        std::memcpy(TerrainMappingLayer1, files.baseTiles.data(), CELLS);
        std::memcpy(TerrainMappingLayer2, files.overlayTiles.data(), CELLS);
        std::memcpy(TerrainMappingAlpha, files.overlayAlpha.data(), CELLS * sizeof(float));
        return;
    case SaveUnit::Height:
        std::memcpy(BackTerrainHeight, files.heights.data(), CELLS * sizeof(float));
        CreateTerrainNormal();
        CreateTerrainLight();
        return;
    case SaveUnit::Attribute:
        for (std::size_t i = 0; i < CELLS; ++i)
            TerrainWall[i] = files.walls[i];
        g_MapEditHistory.Terrain().Changed(MAP_LAYER_WALL, Editor::MapInspect::WholeMap());
        return;
    case SaveUnit::Light:
        std::memcpy(&TerrainLight[0][0], files.light.data(), files.light.size() * sizeof(float));
        CreateTerrainLight();
        return;
    case SaveUnit::Objects:
        ReloadObjects(files.objectFile);
        return;
    }
}

bool SaveUnitFile(SaveUnit unit, int world, SavedUnit& result)
{
    switch (unit)
    {
    case SaveUnit::Texture:
        return Editor::MapSave::SaveMappingEncrypted(world, world, result.report, &result.file);
    case SaveUnit::Height:
        return Editor::HeightSave::Save(world, result.report, &result.file);
    case SaveUnit::Attribute:
        return Editor::AttrSave::SaveClientAtt(world, world, result.report, &result.file);
    case SaveUnit::Light:
        return Editor::LightSave::Save(world, result.report, &result.file);
    case SaveUnit::Objects:
        return Editor::ObjectPlace::Save(world, result.report, &result.file);
    }
    return false;
}
} // namespace

std::vector<SavedUnit> Save(const std::vector<SaveUnit>& units)
{
    Editor::LiveMap::SyncWithLoadedMap();
    const int world = Editor::LiveMap::WorldFolder();
    std::vector<SavedUnit> results;
    for (SaveUnit unit : units)
    {
        SavedUnit result;
        result.unit = unit;
        result.saved = SaveUnitFile(unit, world, result);
        results.push_back(std::move(result));
    }
    return results;
}

bool Revert(const std::vector<SaveUnit>& units, std::string& error)
{
    g_MapEditorUI.ForgetUnloadedMap();
    const int world = Editor::LiveMap::WorldFolder();
    DecodedFiles files;
    for (SaveUnit unit : units)
    {
        if (!Decode(unit, world, files, error))
            return false;
    }
    for (SaveUnit unit : units)
        Restore(unit, files);
    for (SaveUnit unit : units)
        Editor::LiveMap::NoteSaved(unit, world);
    g_MapEditorUI.ForgetEdits();
    g_MuEditorConsoleUI.LogEditor("[MapEditor] map-revert: reloaded " + std::to_string(units.size()) +
                                  " file(s) of World" + std::to_string(world) + "; the undo history was cleared");
    return true;
}
} // namespace Editor::LiveMapFiles

#endif // _EDITOR
