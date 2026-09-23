#pragma once

#ifdef _EDITOR

#include <string>

// The direction an arriving player faces, as Gate.bmd's arrival records and OpenMU's
// ExitGate.Direction store it (the same numbers): 0 undefined, 1 west, 2 south-west,
// 3 south, 4 south-east, 5 east, 6 north-east, 7 north, 8 north-west. The names are
// OpenMU's.
namespace Editor::Gates
{
// "undefined", "west", "southwest", ... "northwest"; "?" outside 0..8.
const char* DirectionName(int direction);

// A name above (any case, with or without a '-' or '_': "north-west") or a number 0..8.
bool DirectionFromName(const std::string& name, int& direction);

// "undefined, west, southwest, south, southeast, east, northeast, north, northwest".
std::string DirectionNames();
} // namespace Editor::Gates

#endif // _EDITOR
