#pragma once

#ifdef _EDITOR

#include <string>

namespace Editor::MapInspect
{
// Marks an object the map file did not place (added in the editor since the load).
constexpr int NOT_FROM_FILE = -1;

// One world object of the loaded map (a tree, a house, a fence...) as the map's
// object file (EncTerrain{N}.obj) stores it, with what the editor knows about it.
struct MapObjectRecord
{
    int loadedRecord = NOT_FROM_FILE; // its record index in the file the map loaded
    int type = 0;                     // model type
    std::string name;                 // the asset catalog's name, else the model's own
    float position[3] = {};           // world units
    float angle[3] = {};              // degrees
    float scale = 1.0f;
    int block = 0;             // the 16 x 16-tile object-grid block it is kept in (x block * 16 + y block)
    float groundHeight = 0.0f; // the terrain height under it
};
} // namespace Editor::MapInspect

#endif // _EDITOR
