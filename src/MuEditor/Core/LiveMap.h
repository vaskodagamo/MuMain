#pragma once

#ifdef _EDITOR

#include "MapInspect/MapDigest.h"
#include "MapInspect/MapExport.h"
#include "MapInspect/MapObjectRecord.h"
#include "MapInspect/TerrainView.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

// The map the client has loaded, as the inspection units (MapInspect/) read it: the
// engine's terrain arrays, world objects, gates and texture slots, and which of the
// map's files hold edits that were not saved yet.
namespace Editor::LiveMap
{
// Data/World{N} of the loaded map; the engine's map index is N - 1.
int WorldFolder();
Editor::MapInspect::MapIdentity Identity();

Editor::MapInspect::TerrainView Terrain();

// The saved world objects (what EncTerrain{N}.obj would hold) in save order, named
// from the world's asset catalog, else from the model file.
std::vector<Editor::MapInspect::MapObjectRecord> Objects();

// The texture file the loader read into each tile slot (empty where it read none).
std::vector<Editor::MapInspect::TileSlotInfo> TileSlots();

// Gate.bmd's gate kinds (GATE_ATTRIBUTE::Flag).
constexpr int GATE_ENTER = 1;   // walking into its area warps to its target gate
constexpr int GATE_ARRIVAL = 2; // where a warp arrives

// A gate of Gate.bmd whose area lies on the loaded map.
struct Gate
{
    int number = 0;
    int kind = 0; // GATE_ENTER, GATE_ARRIVAL or another flag
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
    int target = 0;     // the gate a warp arrives at (kind 1)
    int targetMap = -1; // that gate's map index (kind 1)
    int level = 0;      // the level needed to use it
};
std::vector<Gate> Gates();

// The asset catalog of the loaded world, when there is one.
struct CatalogStatus
{
    bool available = false;
    std::filesystem::path file;
    int models = 0;
    std::string problem; // why there is none, or why it did not load
};
CatalogStatus Catalog();

// Takes the loaded map as the saved state when a new map's objects replaced the old
// ones (Engine ObjectListGeneration). The Map Editor calls it every frame before any
// tool runs; the map-* commands call it before they look at the map.
void SyncWithLoadedMap();
// A save of `unit` to Data/World{world} finished: when that is the loaded map, what
// it holds now is saved.
void NoteSaved(Editor::MapInspect::SaveUnit unit, int world);
// Per unit (MapDigest.h): the loaded map holds edits that were not saved.
std::array<bool, Editor::MapInspect::SAVE_UNIT_COUNT> UnsavedUnits();

// The terrain height at a world position.
float GroundHeight(float x, float y);

// The loaded map's height file stores one byte per corner as height / HeightFactor()
// (1.5; 3.0 on the login scene), so the highest height it keeps is 255 x that.
float HeightFactor();
float MaxStoredHeight();

// Model `type`'s name in the loaded world's asset catalog; empty without one.
std::string CatalogName(int type);
} // namespace Editor::LiveMap

#endif // _EDITOR
