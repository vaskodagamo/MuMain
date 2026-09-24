#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// The names of maps the client has no built-in name for: new maps, numbered from
// World::MapNumbers::FIRST_NEW_MAP (82) to LAST_NEW_MAP (254). Such a map names itself in
// Data/World{map + 1}/MapName.txt, whose first line with text is the name (UTF-8). The
// game's own maps keep their built-in names; a map without the file falls back to them
// (CMapManager::GetMapName).
namespace World::MapNames
{
constexpr const wchar_t* NAME_FILE = L"MapName.txt";
// Longer names are cut (at a character boundary): the HUD shows a short line.
constexpr std::size_t MAX_NAME_BYTES = 64;

// The name a MapName.txt holds: its first line with text, without the spaces around it
// (and a UTF-8 byte order mark), at most MAX_NAME_BYTES bytes. Empty when there is none.
std::string ParseNameFile(std::string_view text);

// The name of `map` from its MapName.txt, read once and then kept; nullptr for the
// game's own map numbers, numbers above LAST_NEW_MAP and maps without a name file.
const wchar_t* Find(int map);

// Reads the name file of `map` again the next time it is asked for (the Map Editor
// wrote it).
void Forget(int map);
} // namespace World::MapNames
