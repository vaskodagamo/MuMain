#pragma once

#ifdef _EDITOR

#include "EditCommand.h"
#include "TerrainLayers.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Editor::Editing
{
// The bytes of one layer inside a command's rectangle, row by row, before and
// after the stroke.
struct LayerPatch
{
    int layer = 0;
    std::vector<std::uint8_t> before;
    std::vector<std::uint8_t> after;
};

// Copies `rect` out of a whole layer (`width` cells per row, `elementBytes` per
// cell), row by row.
std::vector<std::uint8_t> ReadRect(const std::uint8_t* layerData, int width, std::size_t elementBytes,
                                   const CellRect& rect);
// The same for the live array of `layer`, and the way back into it.
std::vector<std::uint8_t> ReadRect(TerrainLayers& layers, int layer, const CellRect& rect);
void WriteRect(TerrainLayers& layers, int layer, const CellRect& rect, const std::vector<std::uint8_t>& bytes);

// A finished terrain brush stroke: the rectangle it changed and, for each layer it
// painted, that rectangle's bytes before and after.
class TerrainPatchCommand : public EditCommand
{
public:
    TerrainPatchCommand(std::string label, TerrainLayers& layers, CellRect rect, std::vector<LayerPatch> patches);

    bool Undo() override;
    bool Redo() override;
    std::size_t MemoryBytes() const override;

    const CellRect& Rect() const
    {
        return m_rect;
    }

private:
    void Write(bool after);

    TerrainLayers& m_layers;
    CellRect m_rect;
    std::vector<LayerPatch> m_patches;
};
} // namespace Editor::Editing

#endif // _EDITOR
