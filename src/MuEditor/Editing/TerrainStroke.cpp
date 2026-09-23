#include "TerrainStroke.h"

#ifdef _EDITOR

#include "TerrainPatchCommand.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace Editor::Editing
{
void TerrainStroke::Begin(TerrainLayers& layers, std::vector<int> layerIds, std::string label)
{
    m_layers = &layers;
    m_layerIds = std::move(layerIds);
    m_label = std::move(label);
    m_snapshots.resize(m_layerIds.size());
    const std::size_t cells = static_cast<std::size_t>(layers.Width()) * static_cast<std::size_t>(layers.Height());
    for (std::size_t i = 0; i < m_layerIds.size(); ++i)
    {
        const std::uint8_t* data = layers.Data(m_layerIds[i]);
        m_snapshots[i].assign(data, data + cells * layers.ElementBytes(m_layerIds[i]));
    }
}

bool TerrainStroke::IsActive() const
{
    return m_layers != nullptr;
}

void TerrainStroke::Cancel()
{
    m_layers = nullptr;
}

bool TerrainStroke::CellChanged(std::size_t layerIndex, std::size_t cell) const
{
    const std::size_t bytes = m_layers->ElementBytes(m_layerIds[layerIndex]);
    const std::uint8_t* live = m_layers->Data(m_layerIds[layerIndex]) + cell * bytes;
    return std::memcmp(live, m_snapshots[layerIndex].data() + cell * bytes, bytes) != 0;
}

CellRect TerrainStroke::ChangedRect() const
{
    CellRect rect{m_layers->Width(), m_layers->Height(), -1, -1};
    const int width = m_layers->Width();
    for (std::size_t layerIndex = 0; layerIndex < m_layerIds.size(); ++layerIndex)
    {
        for (int y = 0; y < m_layers->Height(); ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t cell = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + x;
                if (!CellChanged(layerIndex, cell))
                    continue;
                rect.minX = std::min(rect.minX, x);
                rect.minY = std::min(rect.minY, y);
                rect.maxX = std::max(rect.maxX, x);
                rect.maxY = std::max(rect.maxY, y);
            }
        }
    }
    return rect;
}

std::unique_ptr<EditCommand> TerrainStroke::Finish()
{
    if (m_layers == nullptr)
        return nullptr;
    TerrainLayers& layers = *m_layers;
    const CellRect rect = ChangedRect();
    m_layers = nullptr;
    if (rect.IsEmpty())
        return nullptr;

    std::vector<LayerPatch> patches;
    for (std::size_t i = 0; i < m_layerIds.size(); ++i)
    {
        LayerPatch patch;
        patch.layer = m_layerIds[i];
        patch.before = ReadRect(m_snapshots[i].data(), layers.Width(), layers.ElementBytes(patch.layer), rect);
        patch.after = ReadRect(layers, patch.layer, rect);
        patches.push_back(std::move(patch));
    }
    return std::make_unique<TerrainPatchCommand>(m_label, layers, rect, std::move(patches));
}
} // namespace Editor::Editing

#endif // _EDITOR
