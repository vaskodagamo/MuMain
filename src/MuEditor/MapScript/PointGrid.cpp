#include "PointGrid.h"

#ifdef _EDITOR

#include <algorithm>
#include <cmath>

namespace Editor::MapScript
{
namespace
{
// Keeps a zero or negative bucket size from dividing by zero.
constexpr float SMALLEST_BUCKET = 0.25f;
// Bucket keys pack the bucket column into the high 32 bits.
constexpr int KEY_SHIFT = 32;
constexpr std::int64_t LOW_MASK = 0xFFFFFFFFll;
} // namespace

PointGrid::PointGrid(float bucketSize) : m_bucketSize(std::max(bucketSize, SMALLEST_BUCKET)) {}

std::int64_t PointGrid::BucketKey(int bucketX, int bucketY) const
{
    return (static_cast<std::int64_t>(bucketX) << KEY_SHIFT) | (static_cast<std::int64_t>(bucketY) & LOW_MASK);
}

int PointGrid::BucketOf(float coordinate) const
{
    return static_cast<int>(std::floor(coordinate / m_bucketSize));
}

void PointGrid::Insert(Point point)
{
    m_buckets[BucketKey(BucketOf(point.x), BucketOf(point.y))].push_back(point);
    ++m_count;
}

bool PointGrid::AnyCloserThan(Point point, float distance) const
{
    if (distance <= 0.0f || m_count == 0)
        return false;
    const int reach = static_cast<int>(std::ceil(distance / m_bucketSize));
    const int centreX = BucketOf(point.x);
    const int centreY = BucketOf(point.y);
    const float limit = distance * distance;
    for (int bucketY = centreY - reach; bucketY <= centreY + reach; ++bucketY)
    {
        for (int bucketX = centreX - reach; bucketX <= centreX + reach; ++bucketX)
        {
            const auto bucket = m_buckets.find(BucketKey(bucketX, bucketY));
            if (bucket == m_buckets.end())
                continue;
            for (const Point& other : bucket->second)
            {
                const float dx = other.x - point.x;
                const float dy = other.y - point.y;
                if (dx * dx + dy * dy < limit)
                    return true;
            }
        }
    }
    return false;
}
} // namespace Editor::MapScript

#endif // _EDITOR
