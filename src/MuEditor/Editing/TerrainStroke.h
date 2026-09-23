#pragma once

#ifdef _EDITOR

#include "EditCommand.h"
#include "TerrainLayers.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Editor::Editing
{
// Records one brush stroke on terrain layers for the undo history. Begin copies the
// layers the brush paints; Finish compares them with the live arrays and turns the
// cells that changed into a TerrainPatchCommand covering their bounding rectangle,
// so the painting code never has to report which cells it touched.
class TerrainStroke
{
public:
    // `label` names the step in the undo history, e.g. "Paint texture".
    void Begin(TerrainLayers& layers, std::vector<int> layerIds, std::string label);
    bool IsActive() const;
    // The finished stroke, or nullptr when no cell changed (also when no stroke was
    // active). The stroke ends either way.
    std::unique_ptr<EditCommand> Finish();
    // Ends the stroke without a command (the map it painted was unloaded).
    void Cancel();

private:
    CellRect ChangedRect() const;
    bool CellChanged(std::size_t layerIndex, std::size_t cell) const;

    TerrainLayers* m_layers = nullptr;
    std::vector<int> m_layerIds;
    std::string m_label;
    std::vector<std::vector<std::uint8_t>> m_snapshots; // one per layer id, the whole layer
};
} // namespace Editor::Editing

#endif // _EDITOR
