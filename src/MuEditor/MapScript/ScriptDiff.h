#pragma once

#ifdef _EDITOR

#include "ScriptMap.h"

#include <optional>
#include <vector>

namespace Editor::MapScript
{
// The cells of one terrain layer a script changed.
struct LayerChange
{
    int cells = 0;
    CellRect area; // their bounding rectangle, empty when none changed

    bool Changed() const
    {
        return cells > 0;
    }
};

enum class ObjectChangeKind : std::uint8_t
{
    Added,
    Removed,
    Changed, // moved, turned or scaled
};

struct ObjectDiff
{
    ObjectChangeKind kind = ObjectChangeKind::Added;
    int key = NEW_OBJECT; // the engine's key (NEW_OBJECT for an added object)
    int id = 0;           // the script's id
    std::optional<Editor::Editing::ObjectState> before;
    std::optional<Editor::Editing::ObjectState> after;
};

// What a script changed, bit for bit: per terrain layer, and per object.
struct MapChanges
{
    LayerChange height;
    LayerChange texture1; // the base texture's slots
    LayerChange texture2; // the overlay's slots or opacity
    LayerChange attribute;
    LayerChange light;
    std::vector<ObjectDiff> objects; // removed ones first, then changed, then added (in the order placed)

    bool Any() const;
    int Count(ObjectChangeKind kind) const;
};

MapChanges Diff(const MapState& before, const MapState& after);
} // namespace Editor::MapScript

#endif // _EDITOR
