#pragma once

#ifdef _EDITOR

#include "BlankMap.h"
#include "MapFileCodec.h"

#include <filesystem>
#include <string>
#include <vector>

// What a new map's folders hold, worked out before anything is written: Data/World{N}
// and Data/Object{N} for a new map number (82 to 254; N is the number + 1), copied from
// another map (a template) or made flat (blank). File system reads only.
namespace Editor::NewMap
{
enum class MapSource
{
    Template, // a copy of another map's ground, walk map, light and objects
    Blank,    // one texture, height and walkability everywhere, no objects
};

struct NewMapRequest
{
    int map = 0;      // the game's number of the new map (what OpenMU and Gate.bmd use)
    std::string name; // shown in the game (MapName.txt), UTF-8
    MapSource source = MapSource::Blank;
    int templateWorld = 0;   // Template: the Data/World folder copied
    BlankGround blank;       // Blank: the ground
    int texturesWorld = 1;   // Blank: the Data/World folder whose tile textures (and height file layout) are copied
    int modelsWorld = 0;     // the Data/Object folder whose models are copied; 0 for none
    bool copyMinimap = true; // Template: copy its mini_map.OZT too
};

// One file of the new map; `relative` starts with Data (Data/World83/EncTerrain83.map).
struct PlannedFile
{
    std::filesystem::path relative;
    Bytes bytes;
};

struct NewMapPlan
{
    int map = 0;
    int world = 0;                              // the Data folder number: map + 1
    std::vector<std::filesystem::path> folders; // Data/World{world}, Data/Object{world}
    std::vector<PlannedFile> files;
    std::vector<std::string> warnings;
};

// True for a number a new map may take (82 to 254); otherwise the reason.
bool CheckNewMapNumber(int map, std::string& error);

// Reads what the new map needs from `gameRoot`/Data and fills `plan`. False with the
// reason when the request is not valid or a source file is missing or not one the client
// loads (a template with a 24-bit height file, for example).
bool PlanNewMap(const std::filesystem::path& gameRoot, const NewMapRequest& request, const MapFileCodec& codec,
                NewMapPlan& plan, std::string& error);

// Data/World{world} and Data/Object{world}, relative to the game's folder.
std::filesystem::path WorldFolder(int world);
std::filesystem::path ObjectFolder(int world);
} // namespace Editor::NewMap

#endif // _EDITOR
