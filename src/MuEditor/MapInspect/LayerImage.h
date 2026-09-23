#pragma once

#ifdef _EDITOR

#include "Image.h"
#include "TerrainView.h"
#include "TileArea.h"

#include <array>
#include <cstdint>
#include <string_view>

// One pixel per tile images of a map's layers, the way a scripted client "reads" a
// map. Pixel (column, row) of an image of `area` is tile (area.minX + column,
// area.maxY - row): +x to the right and +y (north) up, as the top-down camera with
// yaw 0 shows the map.
namespace Editor::MapInspect
{
enum class MapLayer : std::uint8_t
{
    Height,    // grey: the lowest height of the area black, the highest white
    Attribute, // RGB: see AttributeColor
    Texture1,  // RGB: layer 1's texture slot, see TileSlotColor
    Texture2,  // RGB: layer 2's texture slot, black where there is none
    Alpha,     // grey: layer 2's opacity, 0..255
    Light,     // RGB: the painted light map
    Objects,   // grey: how many world objects stand on the tile (objects.json has the list)
};

constexpr std::size_t MAP_LAYER_COUNT = 7;

const std::array<MapLayer, MAP_LAYER_COUNT>& AllMapLayers();
// The protocol name: height, attribute, texture1, texture2, alpha, light, objects.
std::string_view MapLayerName(MapLayer layer);
bool MapLayerFromName(std::string_view name, MapLayer& layer);

// The range a height image spans: `min` is black, `max` white.
struct HeightRange
{
    float min = 0.0f;
    float max = 0.0f;

    // World units between two grey levels (0 for a flat area).
    float UnitsPerLevel() const;
};

HeightRange MeasureHeightRange(const TerrainView& terrain, const CellRect& area);

// The grey level of `height` in `range`, rounded; 0 for a flat range.
std::uint8_t HeightLevel(float height, const HeightRange& range);

// The tile a pixel of an image of `area` shows, and back.
void TileOfPixel(const CellRect& area, int column, int row, int& x, int& y);
void PixelOfTile(const CellRect& area, int x, int y, int& column, int& row);

// The image of a terrain layer (not Objects, which needs the object list). Grey
// layers have one channel, the others three.
Image RenderTerrainLayer(const TerrainView& terrain, MapLayer layer, const CellRect& area, const HeightRange& heights);
} // namespace Editor::MapInspect

#endif // _EDITOR
