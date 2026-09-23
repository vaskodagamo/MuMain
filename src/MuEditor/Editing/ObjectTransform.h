#pragma once

#ifdef _EDITOR

#include "ObjectWorld.h"

#include <string>
#include <vector>

// Moving, turning and scaling a group of world objects together, as the transform
// gizmo does: the group turns and scales around a pivot, the centre of its objects.
namespace Editor::Editing::Transform
{
// The smallest scale an edit leaves an object with.
constexpr float MIN_OBJECT_SCALE = 0.05f;
// Snap steps: a quarter tile (a tile is 100 world units), 15 degrees, 0.1 scale.
constexpr float MOVE_SNAP = 25.0f;
constexpr float ROTATE_SNAP_DEGREES = 15.0f;
constexpr float SCALE_SNAP = 0.1f;
// Move::axis for a move in the ground plane (X and Y together).
constexpr int AXIS_XY = 3;

enum class Kind
{
    Move,
    Rotate,
    Scale,
};

// One gizmo drag, measured from where it started.
struct Delta
{
    Kind kind = Kind::Move;
    int axis = 0;         // Move: X, Y, Z or AXIS_XY; Rotate: the world axis X, Y or Z
    float move[3] = {};   // Move, in world units
    float degrees = 0.0f; // Rotate, right-hand rule around the axis
    float factor = 1.0f;  // Scale, uniform
};

// The centre of the objects' positions.
void Pivot(const std::vector<ObjectState>& objects, float out[3]);
// The objects after `delta`: a move shifts them all; a turn and a scale also move
// each position around `pivot`; a turn changes the angles as the engine reads them
// (see Editor::Gizmo::RotateAngles).
std::vector<ObjectState> Apply(const std::vector<ObjectState>& start, const float pivot[3], const Delta& delta);
// `delta` with snapping, measured on `reference` (the primary object at the start
// of the drag): a move puts it on the quarter-tile grid along the moved axes, a turn
// goes in 15-degree steps, and a scale leaves it at a multiple of 0.1.
Delta Snapped(const Delta& delta, const ObjectState& reference);
// The drag as the gizmo's label shows it: "Move 125, 0, 0", "Rotate Z 45.0 deg",
// "Scale x1.20".
std::string Describe(const Delta& delta);
} // namespace Editor::Editing::Transform

#endif // _EDITOR
