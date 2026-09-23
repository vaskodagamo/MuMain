#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "OpReport.h"
#include "ScriptMap.h"

#include <string>

namespace Editor::MapScript::Ops
{
// texture.paint: layer 1 sets the tile slot of every tile whose centre lies inside the
// shape (hard edge); layer 2 paints the overlay with the soft brush of the Texture tab
// (Editing::PaintOverlay) towards `opacity`. texture.erase fades layer 2 out.
bool ApplyTexture(const Op& op, const TextureEdit& edit, MapState& state, OpReport& report, std::string& error);

// attribute.set: the walkability of every tile whose centre lies inside the shape.
// Refused when it would change the map's anti-tamper tile.
bool ApplyAttribute(const Op& op, const AttributeEdit& edit, const MapContext& context, MapState& state,
                    OpReport& report, std::string& error);

// light.add, .subtract, .tint, .set and .smooth on the painted light map (soft edge),
// clamped to 0 .. 1 per channel.
bool ApplyLight(const Op& op, const LightEdit& edit, MapState& state, OpReport& report, std::string& error);

// Where a hard-edged op acts on a tile: its weight reaches this.
constexpr float HARD_EDGE_WEIGHT = 0.5f;

// False, with the reason, when writing `value` on tile (x, y) would change the map's
// anti-tamper tile (Attributes::SentinelFor).
bool SentinelAllows(const Op& op, const MapContext& context, int x, int y, std::uint16_t value, std::string& error);
} // namespace Editor::MapScript::Ops

#endif // _EDITOR
