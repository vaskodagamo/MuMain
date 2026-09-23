#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <doctest.h>

#include "Render/Renderer/DrawCommandHistory.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/QuadTopology.h"

static_assert(requires(mu::IMuRenderer& renderer, std::span<const mu::Vertex3D> vertices)
{
    renderer.RenderQuad3D(vertices, 0u);
});

static_assert(requires(mu::RendererStats stats)
{
    stats.pipelineBinds;
    stats.samplerBinds;
    stats.vertexUniformPushes;
    stats.fragmentUniformPushes;
    stats.merged2DDrawCalls;
});

TEST_CASE("independent quads require four vertices [render][quad]")
{
    CHECK(Render::Topology::IsValidQuadVertexCount(0));
    CHECK(Render::Topology::IsValidQuadVertexCount(4));
    CHECK(Render::Topology::IsValidQuadVertexCount(8));
    CHECK_FALSE(Render::Topology::IsValidQuadVertexCount(3));
    CHECK_FALSE(Render::Topology::IsValidQuadVertexCount(6));
}

TEST_CASE("independent quads use perimeter triangle indices [render][quad]")
{
    std::array<std::uint16_t, 12> indices{};

    Render::Topology::FillQuadIndices(indices);

    CHECK(indices == std::array<std::uint16_t, 12>{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7});
}

TEST_CASE("world-space quads select the 3D pipeline [render][quad]")
{
    CHECK(Render::Topology::Uses3DPipeline(Render::Topology::QuadSpace::World));
    CHECK_FALSE(Render::Topology::Uses3DPipeline(Render::Topology::QuadSpace::Screen));
}

TEST_CASE("adjacent quad draws merge within the static index capacity [render][quad]")
{
    constexpr auto vertexStride = static_cast<std::uint32_t>(sizeof(mu::Vertex3D));
    constexpr std::uint32_t firstOffset = 64;
    constexpr std::uint32_t secondOffset = firstOffset + 4 * vertexStride;

    CHECK(Render::Topology::CanMergeQuadDraws(firstOffset, 6, secondOffset, 6, vertexStride, 4096));
    CHECK_FALSE(Render::Topology::CanMergeQuadDraws(firstOffset, 6, secondOffset + vertexStride, 6, vertexStride,
                                                    4096));
    CHECK_FALSE(Render::Topology::CanMergeQuadDraws(firstOffset, 4096 * 6, secondOffset, 6, vertexStride, 4096));
}

TEST_CASE("adjacent triangle draws require contiguous vertices for their own stride [render][quad]")
{
    constexpr std::uint32_t firstOffset = 64;
    constexpr std::uint32_t firstVertexCount = 6;
    constexpr auto stride2D = static_cast<std::uint32_t>(sizeof(mu::Vertex2D));
    constexpr auto stride3D = static_cast<std::uint32_t>(sizeof(mu::Vertex3D));

    CHECK(Render::Topology::CanMergeTriangleDraws(
        firstOffset, firstVertexCount, firstOffset + firstVertexCount * stride2D, stride2D));
    CHECK(Render::Topology::CanMergeTriangleDraws(
        firstOffset, firstVertexCount, firstOffset + firstVertexCount * stride3D, stride3D));
    CHECK_FALSE(Render::Topology::CanMergeTriangleDraws(
        firstOffset, firstVertexCount, firstOffset + firstVertexCount * stride2D + 1, stride2D));
    CHECK_FALSE(Render::Topology::CanMergeTriangleDraws(firstOffset, firstVertexCount, firstOffset, 0));
}

TEST_CASE("screen-space quads preserve the static index ceiling [render][quad]")
{
    constexpr auto stride = static_cast<std::uint32_t>(sizeof(mu::Vertex2D));
    constexpr std::uint32_t firstOffset = 128;
    constexpr std::uint32_t secondOffset = firstOffset + 4 * stride;

    CHECK(Render::Topology::CanMergeQuadDraws(firstOffset, 6, secondOffset, 6, stride, 4096));
    CHECK_FALSE(Render::Topology::CanMergeQuadDraws(firstOffset, 4096 * 6, secondOffset, 6, stride, 4096));
}

TEST_CASE("quads beyond one draw's capacity are submitted in whole batches, in order [render][quad]")
{
    constexpr std::size_t maxQuads = Render::Topology::MAX_QUADS_PER_DRAW;
    constexpr std::size_t quadCount = 23131; // the attribute overlay's tiles in the offline Lorencia view
    std::vector<mu::Vertex3D> vertices(quadCount * 4);
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        vertices[i].x = static_cast<float>(i);
    }

    std::vector<std::size_t> batchQuads;
    std::size_t nextVertex = 0;
    bool contiguous = true;
    Render::Topology::ForEachQuadBatch(std::span<const mu::Vertex3D>(vertices), maxQuads,
                                       [&](std::span<const mu::Vertex3D> batch)
                                       {
                                           contiguous = contiguous && batch.size() % 4 == 0 &&
                                                        batch.front().x == static_cast<float>(nextVertex);
                                           nextVertex += batch.size();
                                           batchQuads.push_back(batch.size() / 4);
                                       });

    CHECK(contiguous);
    CHECK(nextVertex == vertices.size());
    REQUIRE(batchQuads.size() == 6);
    CHECK(batchQuads.front() == maxQuads);
    CHECK(batchQuads.back() == quadCount - 5 * maxQuads);
}

TEST_CASE("a batch split submits nothing for no quads and one batch up to the capacity [render][quad]")
{
    int calls = 0;
    const auto count = [&calls](std::span<const mu::Vertex3D>) { ++calls; };
    const std::vector<mu::Vertex3D> none;
    const std::vector<mu::Vertex3D> full(Render::Topology::MAX_QUADS_PER_DRAW * 4);
    const std::vector<mu::Vertex3D> oneMore((Render::Topology::MAX_QUADS_PER_DRAW + 1) * 4);

    Render::Topology::ForEachQuadBatch(std::span<const mu::Vertex3D>(none), Render::Topology::MAX_QUADS_PER_DRAW,
                                       count);
    CHECK(calls == 0);
    Render::Topology::ForEachQuadBatch(std::span<const mu::Vertex3D>(full), Render::Topology::MAX_QUADS_PER_DRAW,
                                       count);
    CHECK(calls == 1);
    Render::Topology::ForEachQuadBatch(std::span<const mu::Vertex3D>(oneMore), Render::Topology::MAX_QUADS_PER_DRAW,
                                       count);
    CHECK(calls == 3);
    Render::Topology::ForEachQuadBatch(std::span<const mu::Vertex3D>(full), 0, count);
    CHECK(calls == 3);
}

TEST_CASE("2D and 3D command families retain independent history [render][quad]")
{
    Render::DrawCommandHistory history;
    history.fill(99);

    Render::PreviousDrawCommand(history, Render::DrawCommandFamily::TextTriangles2D) = 7;
    Render::PreviousDrawCommand(history, Render::DrawCommandFamily::ScreenQuads2D) = 8;

    CHECK(Render::PreviousDrawCommand(history, Render::DrawCommandFamily::Triangles3D) == 99);
    CHECK(Render::PreviousDrawCommand(history, Render::DrawCommandFamily::Quads3D) == 99);
    CHECK(Render::PreviousDrawCommand(history, Render::DrawCommandFamily::TextTriangles2D) == 7);
    CHECK(Render::PreviousDrawCommand(history, Render::DrawCommandFamily::ScreenQuads2D) == 8);
}
