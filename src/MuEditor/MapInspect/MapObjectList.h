#pragma once

#ifdef _EDITOR

#include "Image.h"
#include "MapObjectRecord.h"
#include "TileArea.h"

#include <json.hpp>

#include <vector>

// The loaded map's world objects as a scripted client reads them (objects.json,
// map-query).
namespace Editor::MapInspect
{
// Sorts `objects` into the order a save writes them (Engine::Object::WorldObjectFile
// ::InSaveOrder): the ones the file placed by their record index, then the added ones
// in the order given. A position in the sorted list is the record index the object
// gets in the next save.
void SortInSaveOrder(std::vector<MapObjectRecord>& objects);

// The tile an object stands on.
int ObjectTileX(const MapObjectRecord& object);
int ObjectTileY(const MapObjectRecord& object);

// The positions (in `objects`) of the objects standing on a tile of `area`.
std::vector<std::size_t> ObjectsInside(const std::vector<MapObjectRecord>& objects, const CellRect& area);

// One object as objects.json and map-query list it; `index` is its position in save
// order.
nlohmann::json ObjectToJson(const MapObjectRecord& object, std::size_t index);

// A grey image of `area` (orientation as in LayerImage.h): black where no object
// stands, brighter the more objects stand on the tile.
Image RenderObjectOccupancy(const std::vector<MapObjectRecord>& objects, const CellRect& area);

// The grey level RenderObjectOccupancy gives a tile with `count` objects.
std::uint8_t OccupancyLevel(int count);
} // namespace Editor::MapInspect

#endif // _EDITOR
