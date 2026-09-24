#pragma once

#ifdef _EDITOR

#include "EditScript.h"

#include <cstdint>
#include <string>

// How long a script would hold the client. map-apply runs on the main loop: while a
// script runs, the window does not redraw and no other request is served, so a script
// that would take seconds is refused before it starts.
namespace Editor::MapScript
{
// Work units: one terrain cell tested against one segment of a shape's outline (or one
// pass of a per-cell step such as a smoothing pass or a noise octave), about 3 to 4 ns
// each on the owner's Mac.
constexpr std::uint64_t MAX_SCRIPT_COST = 400'000'000; // about 1.5 s

// The units `op` costs: the cells its shape reaches times the outline segments each is
// tested against (a polygon's soft edge also searches its deepest point on a grid of up to
// 257 x 257 points), plus the per-cell passes of smoothing and noise.
std::uint64_t OpCost(const Op& op);

// False with the reason, naming the most expensive op, when the ops cost more than
// MAX_SCRIPT_COST together.
bool CheckScriptCost(const EditScript& script, std::string& error);
} // namespace Editor::MapScript

#endif // _EDITOR
