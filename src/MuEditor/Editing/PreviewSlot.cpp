#include "PreviewSlot.h"

#ifdef _EDITOR

#include <algorithm>
#include <cmath>

namespace Editor::Preview
{
namespace
{
constexpr float PI = 3.14159265f;
constexpr float DEGREES_TO_RADIANS = PI / 180.0f;
constexpr int MIN_CELLS = 1;
} // namespace

SlotProjection SlotInTarget(int targetWidth, int targetHeight, int cellsWide, int cellsHigh,
                            const InventoryMetrics& metrics)
{
    const float width = static_cast<float>(std::max(targetWidth, 1));
    const float height = static_cast<float>(std::max(targetHeight, 1));

    SlotProjection projection;
    projection.slotWidth = static_cast<float>(std::max(cellsWide, MIN_CELLS)) * metrics.cellSize;
    projection.slotHeight = static_cast<float>(std::max(cellsHigh, MIN_CELLS)) * metrics.cellSize;
    projection.pixelsPerUnit =
        std::min(SLOT_FILL * width / projection.slotWidth, SLOT_FILL * height / projection.slotHeight);
    projection.offsetX = 0.5f * (width - projection.slotWidth * projection.pixelsPerUnit);
    projection.offsetY = 0.5f * (height - projection.slotHeight * projection.pixelsPerUnit);

    // In the game one logical unit is (window height / referenceHeight) pixels and
    // the window's height spans itemFov. Keeping the world size of a logical unit
    // at the item's depth the same here keeps the item's size against its slot:
    // tan(fov / 2) = tan(itemFov / 2) * height / (referenceHeight * pixelsPerUnit).
    const float tanItemHalf = std::tan(metrics.itemFovDegrees * 0.5f * DEGREES_TO_RADIANS);
    const float tanHalf = tanItemHalf * height / (metrics.referenceHeight * projection.pixelsPerUnit);
    projection.fovDegrees = 2.0f * std::atan(tanHalf) / DEGREES_TO_RADIANS;
    projection.perspective = tanHalf / (0.5f * height);
    projection.centerX = static_cast<int>(0.5f * width);
    projection.centerY = static_cast<int>(0.5f * height);
    return projection;
}
} // namespace Editor::Preview

#endif // _EDITOR
