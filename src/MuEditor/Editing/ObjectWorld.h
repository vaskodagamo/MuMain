#pragma once

#ifdef _EDITOR

#include <cstring>

namespace Editor::Editing
{
// What an object edit changes about a world object: its model type and the
// transform a record of EncTerrain{N}.obj stores (position in world units, Euler
// angles in degrees as AngleMatrix reads them, uniform scale).
struct ObjectState
{
    int type = 0;
    float position[3] = {};
    float angle[3] = {};
    float scale = 1.0f;
};

// Bit for bit, as the object file stores the values.
inline bool operator==(const ObjectState& a, const ObjectState& b)
{
    return a.type == b.type && std::memcmp(a.position, b.position, sizeof(a.position)) == 0 &&
           std::memcmp(a.angle, b.angle, sizeof(a.angle)) == 0 && std::memcmp(&a.scale, &b.scale, sizeof(a.scale)) == 0;
}

// An object's state with the key that names the object across edits, also after
// it was deleted and created again (see ObjectWorld).
struct KeyedObjectState
{
    int key = 0;
    ObjectState state;
};

// The world objects an ObjectEditCommand applies to. A key names one object for
// the life of the loaded map: an undo that re-creates a deleted object gives it
// its old key back, so later commands still find it.
class ObjectWorld
{
public:
    virtual ~ObjectWorld() = default;

    // Creates an object with `key` (which no live object has) in `state`.
    virtual bool Create(int key, const ObjectState& state) = 0;
    // Deletes the object with `key`. False when there is none.
    virtual bool Remove(int key) = 0;
    // Moves, turns and scales the object with `key` to `state`. False when there is none.
    virtual bool Update(int key, const ObjectState& state) = 0;
};
} // namespace Editor::Editing

#endif // _EDITOR
