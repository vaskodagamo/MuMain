#pragma once

#ifdef _EDITOR

#include "ScriptShape.h" // CellRect

#include <string>
#include <vector>

namespace Editor::MapScript
{
// What one op of a script did: the cells it may have changed, and for object ops how
// many objects it asked for, placed or selected and their ids.
struct OpReport
{
    int index = 0;
    std::string name;
    CellRect area;        // empty when the op changes no terrain layer
    int requested = -1;   // object.scatter: the count asked for (or the density's)
    int placed = -1;      // object.place and object.scatter
    int selected = -1;    // object edits
    std::vector<int> ids; // the ids of the objects placed or selected
    std::vector<std::string> warnings;
};
} // namespace Editor::MapScript

#endif // _EDITOR
