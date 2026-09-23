#pragma once

#ifdef _EDITOR

// The Item Editor preview's inventory view draws the item exactly as the game's
// inventory does (RenderItem3D: per-type offsets in the slot, per-type angle and
// scale, a 1-degree camera), but into a render target instead of the window. The
// game places the item from the slot's position in its logical 640x480 screen and
// the camera's projection factors; this computes the values that put a slot of
// the item's size in the middle of the target, with the item keeping the size it
// has against its slot in the game. Pure math, no renderer.
namespace Editor::Preview
{
// The game's numbers the inventory view is measured in.
struct InventoryMetrics
{
    float cellSize = 0.0f;        // a slot cell in logical units (INVENTORY_SQUARE_WIDTH)
    float referenceHeight = 0.0f; // the logical screen height (REFERENCE_HEIGHT)
    float itemFovDegrees = 0.0f;  // the inventory camera's vertical field of view
};

// How much of the target's shorter side the slot fills.
constexpr float SLOT_FILL = 0.8f;

struct SlotProjection
{
    // Logical units to target pixels: pixel = logical * pixelsPerUnit + offset
    // (the game's g_fScreenRate_x/y and g_fScreenOffset_x/y).
    float pixelsPerUnit = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    // The slot in logical units, as the inventory passes it to RenderItem3D.
    float slotX = 0.0f;
    float slotY = 0.0f;
    float slotWidth = 0.0f;
    float slotHeight = 0.0f;
    // The camera: vertical field of view for the target, the matching
    // CameraState::PerspectiveX/Y and the target's centre in pixels.
    float fovDegrees = 0.0f;
    float perspective = 0.0f;
    int centerX = 0;
    int centerY = 0;
};

SlotProjection SlotInTarget(int targetWidth, int targetHeight, int cellsWide, int cellsHigh,
                            const InventoryMetrics& metrics);
} // namespace Editor::Preview

#endif // _EDITOR
