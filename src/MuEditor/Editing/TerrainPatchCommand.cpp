#include "TerrainPatchCommand.h"

#ifdef _EDITOR

#include <cstring>
#include <utility>

namespace Editor::Editing
{
namespace
{
std::size_t CellOffset(int width, std::size_t elementBytes, int x, int y)
{
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * elementBytes;
}
} // namespace

std::vector<std::uint8_t> ReadRect(const std::uint8_t* layerData, int width, std::size_t elementBytes,
                                   const CellRect& rect)
{
    const std::size_t rowBytes = static_cast<std::size_t>(rect.Width()) * elementBytes;
    std::vector<std::uint8_t> bytes(rowBytes * static_cast<std::size_t>(rect.Height()));
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        const std::size_t row = static_cast<std::size_t>(y - rect.minY);
        std::memcpy(bytes.data() + row * rowBytes, layerData + CellOffset(width, elementBytes, rect.minX, y), rowBytes);
    }
    return bytes;
}

std::vector<std::uint8_t> ReadRect(TerrainLayers& layers, int layer, const CellRect& rect)
{
    return ReadRect(layers.Data(layer), layers.Width(), layers.ElementBytes(layer), rect);
}

void WriteRect(TerrainLayers& layers, int layer, const CellRect& rect, const std::vector<std::uint8_t>& bytes)
{
    const std::size_t elementBytes = layers.ElementBytes(layer);
    const std::size_t rowBytes = static_cast<std::size_t>(rect.Width()) * elementBytes;
    if (bytes.size() != rowBytes * static_cast<std::size_t>(rect.Height()))
        return;
    std::uint8_t* data = layers.Data(layer);
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        const std::size_t row = static_cast<std::size_t>(y - rect.minY);
        std::memcpy(data + CellOffset(layers.Width(), elementBytes, rect.minX, y), bytes.data() + row * rowBytes,
                    rowBytes);
    }
}

TerrainPatchCommand::TerrainPatchCommand(std::string label, TerrainLayers& layers, CellRect rect,
                                         std::vector<LayerPatch> patches)
    : EditCommand(std::move(label)), m_layers(layers), m_rect(rect), m_patches(std::move(patches))
{
}

void TerrainPatchCommand::Write(bool after)
{
    for (const LayerPatch& patch : m_patches)
    {
        WriteRect(m_layers, patch.layer, m_rect, after ? patch.after : patch.before);
        m_layers.Changed(patch.layer, m_rect);
    }
}

bool TerrainPatchCommand::Undo()
{
    Write(false);
    return true;
}

bool TerrainPatchCommand::Redo()
{
    Write(true);
    return true;
}

std::size_t TerrainPatchCommand::MemoryBytes() const
{
    std::size_t bytes = sizeof(*this) + Label().capacity();
    for (const LayerPatch& patch : m_patches)
        bytes += sizeof(patch) + patch.before.capacity() + patch.after.capacity();
    return bytes;
}
} // namespace Editor::Editing

#endif // _EDITOR
