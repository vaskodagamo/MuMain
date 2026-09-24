#pragma once

#ifdef _EDITOR

#include "OpReport.h"
#include "ScriptDiff.h"
#include "ScriptMap.h"

#include "json.hpp"

#include <vector>

namespace Editor::MapScript
{
// map-apply's answer lists at most this many objects and ids per op.
constexpr std::size_t LISTED_OBJECTS = 200;

// What a script did (or, for a dry run, would do) as map-apply answers it: "ops" (each
// op's area, placed/selected counts and ids, warnings), "changes" (cells and bounding
// rectangle per layer, objects added, removed and changed, the first LISTED_OBJECTS of
// them with model, tile and transform) and all "warnings".
nlohmann::json ReportJson(const std::vector<OpReport>& reports, const MapChanges& changes, const MapContext& context);
} // namespace Editor::MapScript

#endif // _EDITOR
