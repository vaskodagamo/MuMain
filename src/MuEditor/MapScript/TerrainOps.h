#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "OpReport.h"
#include "ScriptMap.h"

#include <string>

namespace Editor::MapScript::Ops
{
// terrain.raise, .lower, .flatten, .set, .ramp, .smooth and .noise on `state`'s heights,
// each corner by the shape's weight (soft edge by default), clamped to 0 .. maxHeight.
// With objects_follow, objects standing on ground that moved keep their height above it
// (as the Height tab's "Objects follow terrain"). False with the reason (and `state`
// possibly half changed: the runner works on a copy) when the op cannot run here.
bool ApplyTerrain(const Op& op, const TerrainEdit& edit, const MapContext& context, MapState& state, OpReport& report,
                  std::string& error);

// The height `target` names, read from `terrain` inside `mask` (weighted) or at the
// shape's centre.
float ResolveHeight(const HeightTarget& target, const Shape& shape, const WeightMask& mask, const MapTerrain& terrain,
                    float specialHeight);
} // namespace Editor::MapScript::Ops

#endif // _EDITOR
