#include "MapObjectList.h"

#ifdef _EDITOR

#include "LayerImage.h" // PixelOfTile

#include "Engine/Object/WorldObjectFile.h"

#include <algorithm>
#include <map>
#include <utility>

namespace Editor::MapInspect
{
namespace
{
constexpr int GREY_CHANNELS = 1;
// One object is clearly visible; each further one brightens the tile up to white.
constexpr int FIRST_OBJECT_LEVEL = 160;
constexpr int LEVEL_PER_EXTRA_OBJECT = 40;
constexpr int LEVEL_MAX = 255;

nlohmann::json Triple(const float (&values)[3])
{
    return nlohmann::json::array({values[0], values[1], values[2]});
}
} // namespace

void SortInSaveOrder(std::vector<MapObjectRecord>& objects)
{
    std::vector<int> orders;
    orders.reserve(objects.size());
    for (const MapObjectRecord& object : objects)
        orders.push_back(object.loadedRecord);

    std::vector<MapObjectRecord> sorted;
    sorted.reserve(objects.size());
    for (std::size_t index : Engine::Object::WorldObjectFile::SaveOrderIndices(orders))
        sorted.push_back(std::move(objects[index]));
    objects = std::move(sorted);
}

int ObjectTileX(const MapObjectRecord& object)
{
    return TileOf(object.position[0]);
}

int ObjectTileY(const MapObjectRecord& object)
{
    return TileOf(object.position[1]);
}

std::vector<std::size_t> ObjectsInside(const std::vector<MapObjectRecord>& objects, const CellRect& area)
{
    std::vector<std::size_t> inside;
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        if (Contains(area, ObjectTileX(objects[index]), ObjectTileY(objects[index])))
            inside.push_back(index);
    }
    return inside;
}

nlohmann::json ObjectToJson(const MapObjectRecord& object, std::size_t index)
{
    nlohmann::json entry;
    entry["index"] = index;
    entry["loaded_record"] = object.loadedRecord;
    entry["type"] = object.type;
    entry["name"] = object.name;
    entry["tile"] = nlohmann::json::array({ObjectTileX(object), ObjectTileY(object)});
    entry["position"] = Triple(object.position);
    entry["height"] = object.position[2];
    entry["height_above_ground"] = object.position[2] - object.groundHeight;
    entry["angle"] = Triple(object.angle);
    entry["scale"] = object.scale;
    entry["block"] = object.block;
    return entry;
}

std::uint8_t OccupancyLevel(int count)
{
    if (count <= 0)
        return 0;
    return static_cast<std::uint8_t>(std::min(FIRST_OBJECT_LEVEL + (count - 1) * LEVEL_PER_EXTRA_OBJECT, LEVEL_MAX));
}

Image RenderObjectOccupancy(const std::vector<MapObjectRecord>& objects, const CellRect& area)
{
    if (area.IsEmpty())
        return Image{};

    std::map<std::pair<int, int>, int> counts;
    for (std::size_t index : ObjectsInside(objects, area))
        ++counts[{ObjectTileX(objects[index]), ObjectTileY(objects[index])}];

    Image image = MakeImage(area.Width(), area.Height(), GREY_CHANNELS);
    for (const auto& [tile, count] : counts)
    {
        int column = 0;
        int row = 0;
        PixelOfTile(area, tile.first, tile.second, column, row);
        SetGrey(image, column, row, OccupancyLevel(count));
    }
    return image;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
