#pragma once

#ifdef _EDITOR

#include "ScriptRandom.h"
#include "ScriptShape.h" // Point, Bounds

#include <functional>
#include <vector>

namespace Editor::MapScript
{
// What object.scatter asks for.
struct ScatterSettings
{
    int count = 0;           // how many points to find
    float minSpacing = 0.0f; // no two points closer than this (tiles)
    Bounds bounds;           // where to look
    int maxAttempts = 0;     // candidates to try before giving up
};

// Poisson-disk sampling by dart throwing: candidates are drawn uniformly inside
// `settings.bounds` (three random numbers per candidate, whatever happens to it), and
// one is kept when `keepChance(point)` beats a random number (1 keeps it always, 0
// never: outside the shape, on a road, too steep; a soft shape's edge thins out) and no
// kept point lies closer than minSpacing. Stops at `count` points or after maxAttempts
// candidates, so the same seed and inputs always give the same points in the same
// order. Dart throwing, rather than growing outwards from one seed point, spreads a
// small count over the whole area.
std::vector<Point> ScatterPoints(const ScatterSettings& settings, Random& random,
                                 const std::function<float(Point)>& keepChance);
} // namespace Editor::MapScript

#endif // _EDITOR
