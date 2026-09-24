#include "LayerImage.h"

#ifdef _EDITOR

#include "AttributePalette.h"
#include "TilePalette.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Editor::MapInspect
{
namespace
{
constexpr int GREY_CHANNELS = 1;
constexpr int RGB_CHANNELS = 3;
constexpr float LEVEL_MAX = 255.0f;
constexpr float ROUND_HALF = 0.5f;

struct LayerName
{
    MapLayer layer;
    std::string_view name;
};

constexpr std::array<LayerName, MAP_LAYER_COUNT> LAYER_NAMES = {{
    {MapLayer::Height, "height"},
    {MapLayer::Attribute, "attribute"},
    {MapLayer::Texture1, "texture1"},
    {MapLayer::Texture2, "texture2"},
    {MapLayer::Alpha, "alpha"},
    {MapLayer::Light, "light"},
    {MapLayer::Objects, "objects"},
}};

std::uint8_t UnitToLevel(float value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * LEVEL_MAX + ROUND_HALF);
}

bool IsGreyLayer(MapLayer layer)
{
    return layer == MapLayer::Height || layer == MapLayer::Alpha || layer == MapLayer::Objects;
}

// Writes one pixel of a terrain layer for cell `index`.
void PaintCell(Image& image, int column, int row, const TerrainView& terrain, MapLayer layer, std::size_t index,
               const HeightRange& heights)
{
    switch (layer)
    {
    case MapLayer::Height:
        SetGrey(image, column, row, HeightLevel(terrain.height[index], heights));
        break;
    case MapLayer::Attribute:
        SetRgb(image, column, row, AttributeColor(StoredAttribute(terrain.attribute[index])));
        break;
    case MapLayer::Texture1:
        SetRgb(image, column, row, TileSlotColor(terrain.baseTiles[index]));
        break;
    case MapLayer::Texture2:
        SetRgb(image, column, row, TileSlotColor(terrain.overlayTiles[index]));
        break;
    case MapLayer::Alpha:
        SetGrey(image, column, row, UnitToLevel(terrain.overlayAlpha[index]));
        break;
    case MapLayer::Light:
    {
        const float* rgb = terrain.light + index * RGB_CHANNELS;
        SetRgb(image, column, row, Rgb{UnitToLevel(rgb[0]), UnitToLevel(rgb[1]), UnitToLevel(rgb[2])});
        break;
    }
    case MapLayer::Objects:
        break;
    }
}

bool HasLayerData(const TerrainView& terrain, MapLayer layer)
{
    switch (layer)
    {
    case MapLayer::Height:
        return terrain.height != nullptr;
    case MapLayer::Attribute:
        return terrain.attribute != nullptr;
    case MapLayer::Texture1:
        return terrain.baseTiles != nullptr;
    case MapLayer::Texture2:
        return terrain.overlayTiles != nullptr;
    case MapLayer::Alpha:
        return terrain.overlayAlpha != nullptr;
    case MapLayer::Light:
        return terrain.light != nullptr;
    case MapLayer::Objects:
        return false;
    }
    return false;
}
} // namespace

const std::array<MapLayer, MAP_LAYER_COUNT>& AllMapLayers()
{
    static const std::array<MapLayer, MAP_LAYER_COUNT> layers = []
    {
        std::array<MapLayer, MAP_LAYER_COUNT> ordered{};
        for (std::size_t i = 0; i < LAYER_NAMES.size(); ++i)
            ordered[i] = LAYER_NAMES[i].layer;
        return ordered;
    }();
    return layers;
}

std::string_view MapLayerName(MapLayer layer)
{
    for (const LayerName& entry : LAYER_NAMES)
    {
        if (entry.layer == layer)
            return entry.name;
    }
    return {};
}

bool MapLayerFromName(std::string_view name, MapLayer& layer)
{
    for (const LayerName& entry : LAYER_NAMES)
    {
        if (entry.name == name)
        {
            layer = entry.layer;
            return true;
        }
    }
    return false;
}

float HeightRange::UnitsPerLevel() const
{
    return max > min ? (max - min) / LEVEL_MAX : 0.0f;
}

HeightRange MeasureHeightRange(const TerrainView& terrain, const CellRect& area)
{
    if (terrain.height == nullptr || area.IsEmpty())
        return HeightRange{};

    HeightRange range{std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()};
    for (int y = area.minY; y <= area.maxY; ++y)
    {
        for (int x = area.minX; x <= area.maxX; ++x)
        {
            const float height = terrain.height[CellIndex(x, y)];
            range.min = std::min(range.min, height);
            range.max = std::max(range.max, height);
        }
    }
    return range;
}

std::uint8_t HeightLevel(float height, const HeightRange& range)
{
    if (range.max <= range.min)
        return 0;
    return UnitToLevel((height - range.min) / (range.max - range.min));
}

void TileOfPixel(const CellRect& area, int column, int row, int& x, int& y)
{
    x = area.minX + column;
    y = area.maxY - row;
}

void PixelOfTile(const CellRect& area, int x, int y, int& column, int& row)
{
    column = x - area.minX;
    row = area.maxY - y;
}

Image RenderTerrainLayer(const TerrainView& terrain, MapLayer layer, const CellRect& area, const HeightRange& heights)
{
    if (area.IsEmpty() || !HasLayerData(terrain, layer))
        return Image{};

    Image image = MakeImage(area.Width(), area.Height(), IsGreyLayer(layer) ? GREY_CHANNELS : RGB_CHANNELS);
    for (int row = 0; row < image.height; ++row)
    {
        for (int column = 0; column < image.width; ++column)
        {
            int x = 0;
            int y = 0;
            TileOfPixel(area, column, row, x, y);
            PaintCell(image, column, row, terrain, layer, CellIndex(x, y), heights);
        }
    }
    return image;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
