#pragma once

#ifdef _EDITOR

#include "Editing/FieldBrush.h"   // FloatField
#include "Editing/ObjectWorld.h"  // ObjectState
#include "Editing/SurfaceBrush.h" // OverlayLayer
#include "MapInspect/MapExport.h" // TileSlotInfo
#include "MapInspect/TerrainView.h"
#include "ScriptShape.h" // Point

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Editor::MapScript
{
// A copy of a map's terrain layers that an edit script changes (256 x 256 cells each,
// row by row, as MapInspect::TerrainView describes them).
struct MapTerrain
{
    std::vector<std::uint8_t> baseTiles;
    std::vector<std::uint8_t> overlayTiles;
    std::vector<float> overlayAlpha;
    std::vector<float> height;
    std::vector<std::uint16_t> attribute;
    std::vector<float> light; // three floats per cell

    static MapTerrain CopyOf(const Editor::MapInspect::TerrainView& view);
    // A flat map: every layer filled as a freshly loaded empty map would be.
    static MapTerrain Filled(std::uint8_t baseTile, float height, float light);

    Editor::MapInspect::TerrainView View() const;
    Editor::Editing::FloatField Heights();
    Editor::Editing::FloatField Light();
    Editor::Editing::OverlayLayer Overlay();
};

// Marks an object the script placed (it has no engine key yet).
constexpr int NEW_OBJECT = -1;

// A world object as a script sees it: `key` names it in the engine (OBJECT::SaveOrder,
// NEW_OBJECT for one the script placed), `id` in the script (its index in save order
// when the script started, as map-query and objects.json report it; placed objects get
// the ids after the last one).
struct MapObject
{
    int key = NEW_OBJECT;
    int id = 0;
    Editor::Editing::ObjectState state;
    int placedByOp = -1; // the op that placed it, -1 for objects the map had
};

// A model the map has loaded, so objects of it can be placed.
struct ModelInfo
{
    int type = 0;
    std::string modelName;   // the BMD's own name
    std::string catalogName; // the asset catalog's name, empty without a catalog
};

// Where a map's heights stop: its height file stores one byte per corner as
// height / 1.5 (3.0 on the login scene), so nothing above 255 x that survives a save.
constexpr float DEFAULT_MAX_HEIGHT = 255.0f * 1.5f;
// RequestTerrainHeight's height on tiles with the special-height attribute.
constexpr float DEFAULT_SPECIAL_HEIGHT = 1200.0f;

// What the loaded map offers a script, and its limits.
struct MapContext
{
    int mapIndex = 0; // the engine's map number (world folder - 1), for the walkability checks
    std::vector<ModelInfo> models;
    std::vector<Editor::MapInspect::TileSlotInfo> tileSlots; // empty file: nothing loaded there
    float maxHeight = DEFAULT_MAX_HEIGHT;
    float specialHeight = DEFAULT_SPECIAL_HEIGHT;
    bool heightsEditable = true; // false on maps whose height file stores 24 bits a corner
    std::size_t maxObjects = 0;  // what the object file holds
};

// The map a script edits.
struct MapState
{
    MapTerrain terrain;
    std::vector<MapObject> objects; // in save order
    int nextId = 0;                 // the id the next placed object gets
};

// The height of the ground at a world position, exactly as the engine's
// RequestTerrainHeight computes it: 0 off the map's low edges, `specialHeight` on tiles
// with the special-height attribute, otherwise blended from the four corners of the tile.
float GroundHeight(const MapTerrain& terrain, float worldX, float worldY, float specialHeight);

// The steepest slope of the ground in the tile under a point, in degrees.
float SlopeDegrees(const MapTerrain& terrain, Point point);

// The ground's unit normal at corner (x, y), from the heights of the corners around it
// (the map's edge repeats its outer row and column).
struct Normal
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 1.0f;
};
Normal CornerNormal(const MapTerrain& terrain, int x, int y);

// World units per tile.
constexpr float TILE_WORLD = Editor::MapInspect::TILE_WORLD_SIZE;
} // namespace Editor::MapScript

#endif // _EDITOR
