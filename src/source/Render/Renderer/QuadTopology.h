#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Render::Topology
{

enum class QuadSpace
{
    Screen,
    World,
};

[[nodiscard]] constexpr bool Uses3DPipeline(QuadSpace space)
{
    return space == QuadSpace::World;
}

// The most quads the SDL GPU renderer draws from one RenderQuad2D/RenderQuad3D call (its
// shared quad index buffer holds this many); it cuts a larger call off there.
inline constexpr std::size_t MAX_QUADS_PER_DRAW = 4096;

// Hands `vertices` (whole quads, 4 vertices each) to `submit` in consecutive runs of at
// most `maxQuads` quads, in order, so a caller with more quads than one draw holds still
// gets every one of them drawn.
template <typename Vertex, typename Submit>
void ForEachQuadBatch(std::span<const Vertex> vertices, std::size_t maxQuads, Submit&& submit)
{
    constexpr std::size_t verticesPerQuad = 4;
    if (maxQuads == 0)
    {
        return;
    }

    const std::size_t batchVertices = maxQuads * verticesPerQuad;
    for (std::size_t first = 0; first < vertices.size(); first += batchVertices)
    {
        const std::size_t count = vertices.size() - first < batchVertices ? vertices.size() - first : batchVertices;
        submit(vertices.subspan(first, count));
    }
}

[[nodiscard]] constexpr bool IsValidQuadVertexCount(std::size_t vertexCount)
{
    return vertexCount % 4 == 0;
}

[[nodiscard]] constexpr bool CanMergeTriangleDraws(std::uint32_t previousVertexOffset,
                                                   std::uint32_t previousVertexCount,
                                                   std::uint32_t nextVertexOffset,
                                                   std::uint32_t vertexStride)
{
    if (previousVertexCount == 0 || vertexStride == 0)
    {
        return false;
    }

    const std::uint64_t previousEnd = static_cast<std::uint64_t>(previousVertexOffset) +
                                      static_cast<std::uint64_t>(previousVertexCount) * vertexStride;
    return previousEnd == nextVertexOffset;
}

[[nodiscard]] constexpr bool CanMergeQuadDraws(std::uint32_t previousVertexOffset,
                                               std::uint32_t previousIndexCount,
                                               std::uint32_t nextVertexOffset,
                                               std::uint32_t nextIndexCount,
                                               std::uint32_t vertexStride,
                                               std::uint32_t maxQuads)
{
    constexpr std::uint32_t verticesPerQuad = 4;
    constexpr std::uint32_t indicesPerQuad = 6;

    if (previousIndexCount == 0 || nextIndexCount == 0 || vertexStride == 0 ||
        previousIndexCount % indicesPerQuad != 0 || nextIndexCount % indicesPerQuad != 0)
    {
        return false;
    }

    const std::uint64_t mergedIndexCount = static_cast<std::uint64_t>(previousIndexCount) + nextIndexCount;
    if (mergedIndexCount > static_cast<std::uint64_t>(maxQuads) * indicesPerQuad)
    {
        return false;
    }

    const std::uint64_t previousVertexCount =
        static_cast<std::uint64_t>(previousIndexCount / indicesPerQuad) * verticesPerQuad;
    const std::uint64_t previousEnd =
        static_cast<std::uint64_t>(previousVertexOffset) + previousVertexCount * vertexStride;
    return previousEnd == nextVertexOffset;
}

inline void FillQuadIndices(std::span<std::uint16_t> indices)
{
    for (std::size_t quad = 0; quad < indices.size() / 6; ++quad)
    {
        const auto base = static_cast<std::uint16_t>(quad * 4);
        const std::size_t output = quad * 6;
        indices[output + 0] = base + 0;
        indices[output + 1] = base + 1;
        indices[output + 2] = base + 2;
        indices[output + 3] = base + 0;
        indices[output + 4] = base + 2;
        indices[output + 5] = base + 3;
    }
}

} // namespace Render::Topology
