#pragma once

#ifdef _EDITOR

#include "TerrainBrush.h"

namespace Editor::Editing
{
// Values stored per grid corner, `channels` floats each, row by row: the terrain's
// heights (one channel) or its painted light (red, green, blue, 0..1).
struct FloatField
{
    float* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 1;
};

// The soft brushes below act on the corners inside `circle`, each by its weight
// (see Falloff), stop at the map's edges, and return the rectangle they may have
// changed (empty when the circle lies outside the map). One call is one frame of a
// held stroke.

// Adds amount[c] * weight to each value (a negative amount lowers or darkens).
CellRect AddToField(const FloatField& field, const BrushCircle& circle, const float* amount);

// Moves each value towards target[c] by `rate` (0..1) times its weight of the gap:
// with rate 1 the brush's full-strength core lands exactly on the target.
CellRect MoveFieldToward(const FloatField& field, const BrushCircle& circle, const float* target, float rate);

// Moves each value towards the average of itself and its four neighbours (the
// legacy S6 editor's 5-point kernel) by `rate` times its weight. The averages are
// taken from the values before this call, so the result does not depend on the
// order the corners are visited in; at the map's edges a missing neighbour counts
// as the corner itself.
CellRect SmoothField(const FloatField& field, const BrushCircle& circle, float rate);

// Keeps every value inside `rect` within [low, high].
void ClampField(const FloatField& field, const CellRect& rect, float low, float high);
} // namespace Editor::Editing

#endif // _EDITOR
