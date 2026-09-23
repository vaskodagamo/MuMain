#pragma once

#include <cstddef>

namespace Render::Topology
{

// The OpenGL line primitives the immediate-mode shim accepts.
enum class LineMode
{
    Lines,     // GL_LINES: vertices 0-1, 2-3, ...; an odd last vertex is ignored
    LineStrip, // GL_LINE_STRIP: 0-1, 1-2, ..., (n-2)-(n-1)
    LineLoop,  // GL_LINE_LOOP: the strip plus (n-1)-0
};

// Calls emit(first, second) with the vertex indices of each segment `mode` draws
// through `vertexCount` vertices, in drawing order.
template <typename Emit> void ForEachLineSegment(LineMode mode, std::size_t vertexCount, Emit emit)
{
    if (vertexCount < 2)
    {
        return;
    }

    if (mode == LineMode::Lines)
    {
        for (std::size_t i = 0; i + 1 < vertexCount; i += 2)
        {
            emit(i, i + 1);
        }
        return;
    }

    for (std::size_t i = 0; i + 1 < vertexCount; ++i)
    {
        emit(i, i + 1);
    }

    constexpr std::size_t MIN_LOOP_VERTICES = 3; // two vertices close into the segment already drawn
    if (mode == LineMode::LineLoop && vertexCount >= MIN_LOOP_VERTICES)
    {
        emit(vertexCount - 1, std::size_t{0});
    }
}

} // namespace Render::Topology
