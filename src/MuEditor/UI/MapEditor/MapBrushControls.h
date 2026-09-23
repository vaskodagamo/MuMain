#pragma once

#ifdef _EDITOR

#include "Editing/TerrainBrush.h"

#include <cstdint>

// The cursor on the ground as the terrain brushes see it this frame.
struct TerrainBrushInput
{
    bool onGround = false; // over the terrain and not over an editor window
    float groundX = 0.0f;  // the ground point under the cursor, world units
    float groundY = 0.0f;
    bool leftDown = false;
    bool rightDown = false;
    bool altDown = false;
};

// What the round brushes of the Texture (overlay), Height, Attribute and Light tabs
// share: the size and strength sliders, their [ and ] keys, the brush circle at the
// cursor and its outline on the ground.
namespace Editor::BrushControls
{
struct Range
{
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.1f; // what one [ or ] press changes
};

// Radius in tiles, 0.5 to 20, half a tile per key press.
constexpr Range RADIUS_RANGE = {0.5f, 20.0f, 0.5f};

// The "Radius" slider (in tiles).
void RenderRadius(float& radius);
// A "Strength" slider over `range`, shown with `format` (printf style).
void RenderStrength(float& strength, const Range& range, const char* format);
// [ and ] step the radius, Shift+[ and Shift+] the strength (nothing while typing).
void ApplyKeys(float& radius, float* strength, const Range& strengthRange);

// The brush at the cursor, in cells.
Editor::Editing::BrushCircle CircleAt(const TerrainBrushInput& input, float radius);
// Draws the brush's outline on the ground this frame: a soft brush also shows where
// it starts to fade.
void ShowOutline(const TerrainBrushInput& input, float radius, bool soft, std::uint32_t color);
} // namespace Editor::BrushControls

#endif // _EDITOR
