#pragma once

#ifdef _EDITOR

#include "ScriptShape.h" // Point

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Editor::MapScript
{
// Points on the map sorted into square buckets, so "is any point closer than r?" looks
// at the few buckets around the question instead of at every point.
class PointGrid
{
public:
    // `bucketSize` in tiles; the questions are fastest when r is about this size.
    explicit PointGrid(float bucketSize);

    void Insert(Point point);
    // True when a point of the grid lies closer to `point` than `distance`.
    bool AnyCloserThan(Point point, float distance) const;
    std::size_t Size() const
    {
        return m_count;
    }

private:
    std::int64_t BucketKey(int bucketX, int bucketY) const;
    int BucketOf(float coordinate) const;

    float m_bucketSize;
    std::size_t m_count = 0;
    std::unordered_map<std::int64_t, std::vector<Point>> m_buckets;
};
} // namespace Editor::MapScript

#endif // _EDITOR
