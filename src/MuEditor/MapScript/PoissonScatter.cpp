#include "PoissonScatter.h"

#ifdef _EDITOR

#include "PointGrid.h"

#include <cstddef>

namespace Editor::MapScript
{
namespace
{
// Bucket size when no spacing is asked for.
constexpr float DEFAULT_BUCKET = 1.0f;
} // namespace

std::vector<Point> ScatterPoints(const ScatterSettings& settings, Random& random,
                                 const std::function<float(Point)>& keepChance)
{
    std::vector<Point> points;
    if (settings.count <= 0 || settings.bounds.IsEmpty())
        return points;
    points.reserve(static_cast<std::size_t>(settings.count));
    PointGrid kept(settings.minSpacing > 0.0f ? settings.minSpacing : DEFAULT_BUCKET);
    for (int attempt = 0; attempt < settings.maxAttempts && static_cast<int>(points.size()) < settings.count; ++attempt)
    {
        const Point candidate{random.Range(settings.bounds.minX, settings.bounds.maxX),
                              random.Range(settings.bounds.minY, settings.bounds.maxY)};
        const float chance = random.Uniform();
        if (chance >= keepChance(candidate) || kept.AnyCloserThan(candidate, settings.minSpacing))
            continue;
        kept.Insert(candidate);
        points.push_back(candidate);
    }
    return points;
}
} // namespace Editor::MapScript

#endif // _EDITOR
