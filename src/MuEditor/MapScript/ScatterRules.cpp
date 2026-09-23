#include "ScatterRules.h"

#ifdef _EDITOR

#include "AttributeRules.h"
#include "SurfaceOps.h" // SentinelAllows

#include <cmath>
#include <string>

namespace Editor::MapScript::Ops
{
namespace
{
// Bucket size of the existing objects' grid when no distance is asked for.
constexpr float DEFAULT_BUCKET = 1.0f;

Point TilePoint(const Editor::Editing::ObjectState& state)
{
    return Point{state.position[0] / TILE_WORLD, state.position[1] / TILE_WORLD};
}

bool OnMap(int x, int y)
{
    return x >= 0 && y >= 0 && x < Editor::MapInspect::MAP_TILES && y < Editor::MapInspect::MAP_TILES;
}
} // namespace

ScatterRules::ScatterRules(const Op& op, const ScatterEdit& edit, const MapContext& context, const MapState& state)
    : m_op(op), m_edit(edit), m_context(context), m_state(state),
      m_falloff(EffectiveFalloff(edit.shape, EdgeDefault::Hard)),
      m_existing(edit.avoid.objectDistance.value_or(DEFAULT_BUCKET))
{
    for (const Shape& area : edit.avoid.areas)
        m_areaBounds.push_back(ShapeBounds(area));
    if (!edit.avoid.objectDistance)
        return;
    for (const MapObject& object : state.objects)
        m_existing.Insert(TilePoint(object.state));
}

float ScatterRules::KeepChance(Point point) const
{
    const float weight = WeightAt(m_edit.shape, point, m_falloff);
    if (weight <= 0.0f || !TileAllowed(point) || InAvoidedArea(point))
        return 0.0f;
    const AvoidRules& avoid = m_edit.avoid;
    if (avoid.maxSlope && SlopeDegrees(m_state.terrain, point) > *avoid.maxSlope)
        return 0.0f;
    if (avoid.objectDistance && m_existing.AnyCloserThan(point, *avoid.objectDistance))
        return 0.0f;
    return weight;
}

bool ScatterRules::TileAllowed(Point point) const
{
    const int x = static_cast<int>(std::floor(point.x));
    const int y = static_cast<int>(std::floor(point.y));
    if (!OnMap(x, y))
        return false;
    const std::size_t cell = Editor::MapInspect::CellIndex(x, y);
    const std::uint16_t attribute = m_state.terrain.attribute[cell];
    for (std::uint16_t listed : m_edit.avoid.attributes)
    {
        if (Attributes::Matches(attribute, listed))
            return false;
    }
    for (const TileChoice& tile : m_edit.avoid.textures)
    {
        if (m_state.terrain.baseTiles[cell] == tile.slot)
            return false;
    }
    std::string ignored;
    return !m_edit.markAttribute || SentinelAllows(m_op, m_context, x, y, *m_edit.markAttribute, ignored);
}

// Each candidate is tested against every area: the bounds first, so a long road costs its
// segments only near it.
bool ScatterRules::InAvoidedArea(Point point) const
{
    for (std::size_t i = 0; i < m_areaBounds.size(); ++i)
    {
        const Bounds& bounds = m_areaBounds[i];
        const bool nearArea =
            point.x >= bounds.minX && point.x <= bounds.maxX && point.y >= bounds.minY && point.y <= bounds.maxY;
        if (nearArea && InsideDistance(m_edit.avoid.areas[i], point) >= 0.0f)
            return true;
    }
    return false;
}
} // namespace Editor::MapScript::Ops

#endif // _EDITOR
