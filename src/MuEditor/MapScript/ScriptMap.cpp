#include "ScriptMap.h"

#ifdef _EDITOR

#include "MapInspect/AttributeBits.h"
#include "MapInspect/TilePalette.h" // NO_OVERLAY_TILE

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Editor::MapScript
{
namespace
{
namespace Inspect = Editor::MapInspect;

constexpr int TILES = Inspect::MAP_TILES;
constexpr std::size_t CELLS = Inspect::MAP_CELLS;
constexpr int LIGHT_CHANNELS = 3;
constexpr int TILE_MASK = TILES - 1; // TERRAIN_INDEX_REPEAT wraps with it
constexpr float MAP_WORLD_EXTENT = static_cast<float>(TILES) * TILE_WORLD;
constexpr float RADIANS_TO_DEGREES = 180.0f / std::numbers::pi_v<float>;
constexpr float HALF = 0.5f;

template <typename T> std::vector<T> CopyLayer(const T* data, std::size_t count)
{
    return data != nullptr ? std::vector<T>(data, data + count) : std::vector<T>(count, T{});
}

std::size_t RepeatIndex(int x, int y)
{
    return static_cast<std::size_t>(y & TILE_MASK) * TILES + static_cast<std::size_t>(x & TILE_MASK);
}

float CornerHeight(const MapTerrain& terrain, int x, int y)
{
    return terrain.height[Inspect::CellIndex(std::clamp(x, 0, TILES - 1), std::clamp(y, 0, TILES - 1))];
}
} // namespace

MapTerrain MapTerrain::CopyOf(const Inspect::TerrainView& view)
{
    MapTerrain terrain;
    terrain.baseTiles = CopyLayer(view.baseTiles, CELLS);
    terrain.overlayTiles = CopyLayer(view.overlayTiles, CELLS);
    terrain.overlayAlpha = CopyLayer(view.overlayAlpha, CELLS);
    terrain.height = CopyLayer(view.height, CELLS);
    terrain.attribute = CopyLayer(view.attribute, CELLS);
    terrain.light = CopyLayer(view.light, CELLS * LIGHT_CHANNELS);
    return terrain;
}

MapTerrain MapTerrain::Filled(std::uint8_t baseTile, float height, float light)
{
    MapTerrain terrain;
    terrain.baseTiles.assign(CELLS, baseTile);
    terrain.overlayTiles.assign(CELLS, Inspect::NO_OVERLAY_TILE);
    terrain.overlayAlpha.assign(CELLS, 0.0f);
    terrain.height.assign(CELLS, height);
    terrain.attribute.assign(CELLS, 0);
    terrain.light.assign(CELLS * LIGHT_CHANNELS, light);
    return terrain;
}

Inspect::TerrainView MapTerrain::View() const
{
    Inspect::TerrainView view;
    view.baseTiles = baseTiles.data();
    view.overlayTiles = overlayTiles.data();
    view.overlayAlpha = overlayAlpha.data();
    view.height = height.data();
    view.attribute = attribute.data();
    view.light = light.data();
    return view;
}

Editor::Editing::FloatField MapTerrain::Heights()
{
    return {height.data(), TILES, TILES, 1};
}

Editor::Editing::FloatField MapTerrain::Light()
{
    return {light.data(), TILES, TILES, LIGHT_CHANNELS};
}

Editor::Editing::OverlayLayer MapTerrain::Overlay()
{
    return {overlayTiles.data(), overlayAlpha.data(), TILES, TILES};
}

float GroundHeight(const MapTerrain& terrain, float worldX, float worldY, float specialHeight)
{
    if (worldX < 0.0f || worldY < 0.0f)
        return 0.0f;
    const float xf = worldX / TILE_WORLD;
    const float yf = worldY / TILE_WORLD;
    // Far off the map the engine's index lands past the arrays too; it answers the
    // special height there (and this keeps the conversions to int in range).
    if (worldX >= MAP_WORLD_EXTENT * TILES || worldY >= MAP_WORLD_EXTENT)
        return specialHeight;
    const int xi = static_cast<int>(xf);
    const int yi = static_cast<int>(yf);
    const std::size_t index = static_cast<std::size_t>(yi) * TILES + static_cast<std::size_t>(xi);
    if (index >= CELLS || (terrain.attribute[index] & Inspect::Attribute::HEIGHT) == Inspect::Attribute::HEIGHT)
        return specialHeight;

    const float xd = xf - static_cast<float>(xi);
    const float yd = yf - static_cast<float>(yi);
    const float h1 = terrain.height[RepeatIndex(xi, yi)];
    const float h2 = terrain.height[RepeatIndex(xi, yi + 1)];
    const float h3 = terrain.height[RepeatIndex(xi + 1, yi)];
    const float h4 = terrain.height[RepeatIndex(xi + 1, yi + 1)];
    const float left = h1 + (h2 - h1) * yd;
    const float right = h3 + (h4 - h3) * yd;
    return left + (right - left) * xd;
}

float SlopeDegrees(const MapTerrain& terrain, Point point)
{
    const int x = static_cast<int>(std::floor(point.x));
    const int y = static_cast<int>(std::floor(point.y));
    const float h00 = CornerHeight(terrain, x, y);
    const float h10 = CornerHeight(terrain, x + 1, y);
    const float h01 = CornerHeight(terrain, x, y + 1);
    const float h11 = CornerHeight(terrain, x + 1, y + 1);
    const float riseX = ((h10 - h00) + (h11 - h01)) * HALF / TILE_WORLD;
    const float riseY = ((h01 - h00) + (h11 - h10)) * HALF / TILE_WORLD;
    return std::atan(std::hypot(riseX, riseY)) * RADIANS_TO_DEGREES;
}

Normal CornerNormal(const MapTerrain& terrain, int x, int y)
{
    const float riseX = (CornerHeight(terrain, x + 1, y) - CornerHeight(terrain, x - 1, y)) * HALF / TILE_WORLD;
    const float riseY = (CornerHeight(terrain, x, y + 1) - CornerHeight(terrain, x, y - 1)) * HALF / TILE_WORLD;
    const float length = std::sqrt(riseX * riseX + riseY * riseY + 1.0f);
    return Normal{-riseX / length, -riseY / length, 1.0f / length};
}
} // namespace Editor::MapScript

#endif // _EDITOR
