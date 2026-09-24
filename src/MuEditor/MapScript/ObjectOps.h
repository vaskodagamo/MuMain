#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "OpReport.h"
#include "ScriptMap.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Editor::MapScript::Ops
{
// object.place: one object of a loaded model at a point, on the ground unless told
// otherwise.
bool ApplyPlace(const Op& op, const PlaceEdit& edit, const MapContext& context, MapState& state, OpReport& report,
                std::string& error);

// object.scatter: `count` objects (or `density` per tile of the shape) at Poisson-disk
// points with the seed's models, sizes and headings, upright on the ground, where the
// avoid rules allow. Fewer than asked is a warning (the area was full), not an error.
bool ApplyScatter(const Op& op, const ScatterEdit& edit, const MapContext& context, MapState& state, OpReport& report,
                  std::string& error);

// object.move, .rotate, .scale, .delete and .drop_to_ground on the objects the selector
// picks (an error when it picks none, or when a move would take one off the map).
bool ApplyObjectEdit(const Op& op, const ObjectEdit& edit, const MapContext& context, MapState& state, OpReport& report,
                     std::string& error);

// attribute.set with "under": the walkability of the tile each selected object stands on,
// as a scatter's mark_attribute writes it (an error when it picks none, or when a tile is
// the map's anti-tamper tile).
bool MarkUnderObjects(const Op& op, const AttributeEdit& edit, const MapContext& context, MapState& state,
                      OpReport& report, std::string& error);

// The positions (in state.objects) of the objects `selector` picks.
std::vector<std::size_t> Select(const ObjectSelector& selector, const MapState& state);

// Scatter tries this many candidate points per object asked for before it gives up.
constexpr int ATTEMPTS_PER_OBJECT = 60;
} // namespace Editor::MapScript::Ops

#endif // _EDITOR
