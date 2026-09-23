#pragma once

#ifdef _EDITOR

#include "LayerImage.h"
#include "MapObjectRecord.h"

#include <filesystem>
#include <string>
#include <vector>

// map-export: writes the layers of a map (or of a rectangle of it) as one pixel per
// tile PNG images, objects.json and a legend.json that says how to read them.
namespace Editor::MapInspect
{
// A terrain texture slot of the loaded map and the file the loader read into it.
struct TileSlotInfo
{
    int slot = 0;
    std::string file; // as the loader named it, e.g. World1\TileGrass01.jpg; empty when nothing was loaded
};

struct MapIdentity
{
    int world = 0;    // Data/World{world}
    int map = 0;      // the engine's map index (world - 1)
    std::string name; // as the game shows it
};

struct ExportRequest
{
    MapIdentity identity;
    std::vector<MapLayer> layers;
    CellRect area;
    std::filesystem::path folder;
};

struct ExportResult
{
    std::vector<std::filesystem::path> files; // absolute, legend last
};

// Writes the requested layers of `terrain` and `objects` (in save order) into
// request.folder: height.png, attribute.png, texture1.png, texture2.png, alpha.png,
// light.png, objects.png with objects.json, and legend.json. False, with the reason
// in `error`, when a file cannot be written.
bool ExportMap(const TerrainView& terrain, const std::vector<MapObjectRecord>& objects,
               const std::vector<TileSlotInfo>& slots, const ExportRequest& request, ExportResult& result,
               std::string& error);

// The file a layer's image is written to: "<name>.png".
std::string LayerFileName(MapLayer layer);
} // namespace Editor::MapInspect

#endif // _EDITOR
