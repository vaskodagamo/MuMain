#pragma once

// The map numbers the client can use, and the data folders a map's files live in.
//
// A map number travels as one byte where the client reads it (the join, respawn and
// teleport packets, and each record of Data/Gate.bmd), so 255 is the highest. The stock
// client and OpenMU use numbers up to 81 (NUM_WD - 1); 82 and above are free for new
// maps. A map's files live in Data/World{map + 1} and Data/Object{map + 1}; a few event
// maps (Blood Castle, Chaos Castle, Kalima, Illusion Temple) share one folder. The
// EncTerrain files repeat the folder number in one header byte, which the loader checks,
// so the last folder is 255 and the last map that can have files is 254.
namespace World::MapNumbers
{
constexpr int FIRST_NEW_MAP = 82;
constexpr int LAST_MAP = 255;
constexpr int FOLDER_OFFSET = 1;

constexpr int FolderOf(int map)
{
    return map + FOLDER_OFFSET;
}

constexpr int MapOfFolder(int folder)
{
    return folder - FOLDER_OFFSET;
}

constexpr int FIRST_FOLDER = FolderOf(0);
constexpr int LAST_FOLDER = 255;
constexpr int LAST_NEW_MAP = MapOfFolder(LAST_FOLDER);
} // namespace World::MapNumbers
