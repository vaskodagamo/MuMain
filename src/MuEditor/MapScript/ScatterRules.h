#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "PointGrid.h"
#include "ScriptMap.h"

#include <vector>

namespace Editor::MapScript::Ops
{
// Where object.scatter may put an object: the shape's weight at a point (a falloff thins
// the objects towards its edge), or 0 where an avoid rule keeps it out (walkability,
// base texture, slope, other objects, avoided areas) or, for a scatter that marks its
// objects' tiles, where the mark would change the map's anti-tamper tile.
class ScatterRules
{
public:
    // Reads the map as it is now; the objects already there count for `avoid.objects`.
    ScatterRules(const Op& op, const ScatterEdit& edit, const MapContext& context, const MapState& state);

    float KeepChance(Point point) const;

private:
    bool TileAllowed(Point point) const;
    bool InAvoidedArea(Point point) const;

    const Op& m_op;
    const ScatterEdit& m_edit;
    const MapContext& m_context;
    const MapState& m_state;
    float m_falloff;
    PointGrid m_existing;
    std::vector<Bounds> m_areaBounds; // one per avoided area
};
} // namespace Editor::MapScript::Ops

#endif // _EDITOR
