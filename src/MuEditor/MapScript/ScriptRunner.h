#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "OpReport.h"
#include "ScriptMap.h"

#include <string>
#include <vector>

namespace Editor::MapScript
{
// Runs a parsed script on `state`: refuses one that would hold the client too long
// (ScriptCost.h), resolves its model and texture names on the loaded map (`context`),
// then applies the ops in order, each seeing what the ones before it
// did. False with the first error ("ops[4] (object.scatter): ..."); `state` is then half
// edited and must be thrown away, which is why callers run scripts on a copy and apply
// nothing unless the whole script ran.
bool RunScript(EditScript script, const MapContext& context, MapState& state, std::vector<OpReport>& reports,
               std::string& error);
} // namespace Editor::MapScript

#endif // _EDITOR
