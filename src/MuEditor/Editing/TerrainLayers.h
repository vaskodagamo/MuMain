#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <cstdint>

namespace Editor::Editing
{
// A rectangle of terrain cells, both corners included.
struct CellRect
{
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;

    bool IsEmpty() const
    {
        return maxX < minX || maxY < minY;
    }
    int Width() const
    {
        return maxX - minX + 1;
    }
    int Height() const
    {
        return maxY - minY + 1;
    }
};

// The per-cell terrain arrays a brush paints (tile layers, overlay alpha, height,
// walkability), as a TerrainPatchCommand sees them: every layer is Width() x
// Height() cells stored row by row (index y * Width() + x), each cell
// ElementBytes(layer) bytes.
class TerrainLayers
{
public:
    virtual ~TerrainLayers() = default;

    virtual int Width() const = 0;
    virtual int Height() const = 0;
    virtual std::size_t ElementBytes(int layer) const = 0;
    virtual std::uint8_t* Data(int layer) = 0;
    // Called after a command wrote `rect` of `layer`, e.g. to rebuild the lighting
    // after a height change.
    virtual void Changed(int layer, const CellRect& rect) = 0;
};
} // namespace Editor::Editing

#endif // _EDITOR
