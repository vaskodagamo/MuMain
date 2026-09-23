#pragma once

#ifdef _EDITOR

#include "Editing/TerrainBrush.h" // CellRect, CellAnchor, WeightMask

#include <cstdint>
#include <optional>
#include <vector>

// Edit scripts (the control socket's map-apply): high-level map edits an AI agent
// writes as JSON, validated and applied to plain arrays without ImGui or engine state.
// Editor::LiveMapEdit hands them the loaded map and turns the result into one undo step.
namespace Editor::MapScript
{
using Editor::Editing::CellAnchor;
using Editor::Editing::CellRect;
using Editor::Editing::WeightMask;

// A point on the map in tiles: world units / 100. Tile (x, y) covers x..x+1 and
// y..y+1, so its centre is (x + 0.5, y + 0.5); the map runs from 0 to 256.
struct Point
{
    float x = 0.0f;
    float y = 0.0f;
};

// A rectangle in tiles (fractions allowed), for sampling.
struct Bounds
{
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    bool IsEmpty() const
    {
        return maxX <= minX || maxY <= minY;
    }
};

enum class ShapeKind : std::uint8_t
{
    Circle,
    Rect,
    Polygon,
    Path,
};

// Where an edit acts: a circle, a rectangle of whole tiles (both corners included,
// as map-query names them), a polygon, or a path of a given width (a road).
struct Shape
{
    ShapeKind kind = ShapeKind::Circle;
    Point center;              // circle
    float radius = 0.0f;       // circle
    CellRect tiles;            // rect
    std::vector<Point> points; // polygon (3 or more), path (2 or more)
    float width = 0.0f;        // path
    // The soft edge in tiles, measured inwards from the outline: full strength that far
    // inside it, fading to nothing at it. Absent: the op's default (see EdgeDefault).
    std::optional<float> falloff;
};

// How far `point` lies inside the shape's outline, in tiles: above 0 inside, 0 on the
// outline, below 0 outside.
float InsideDistance(const Shape& shape, Point point);

// The shape's greatest inside distance: the radius of a circle, half the width of a
// path or of a rectangle's shorter side, and for a polygon the deepest point of a
// grid of quarter tiles.
float InnerRadius(const Shape& shape);

// What an op does with a shape that gives no falloff. Soft ops (heights, light, the
// overlay texture) fade out over the outer half of the shape's inner radius, the soft
// edge the Map Editor's round brushes have; hard ops (walkability, the base texture,
// scattering) use the outline itself.
enum class EdgeDefault : std::uint8_t
{
    Soft,
    Hard,
};

// The share of the inner radius a soft shape fades over (Editing::FALLOFF_START).
constexpr float DEFAULT_SOFT_SHARE = 1.0f - Editor::Editing::FALLOFF_START;

float EffectiveFalloff(const Shape& shape, EdgeDefault edge);

// The weight at a point `insideDistance` inside the outline: with falloff 0, 1 on and
// inside the outline; otherwise 0 at the outline, 1 from `falloff` inwards, and the
// round brushes' smoothstep in between.
float EdgeWeight(float insideDistance, float falloff);

// The weight of `shape` at a point, with the op's edge.
float WeightAt(const Shape& shape, Point point, float falloff);

// The part of the map (tiles 0 to 256) the shape covers.
Bounds ShapeBounds(const Shape& shape);

// The cells the shape can reach: corners (heights, light, overlay) or tiles
// (walkability, base texture), clipped to the map.
CellRect Footprint(const Shape& shape, CellAnchor anchor);

// Every cell's weight, for the brushes of Editing/FieldBrush and SurfaceBrush.
WeightMask Rasterize(const Shape& shape, CellAnchor anchor, float falloff);

// The number of tiles whose centre lies inside the shape (its area for a density).
int TileArea(const Shape& shape);
} // namespace Editor::MapScript

#endif // _EDITOR
