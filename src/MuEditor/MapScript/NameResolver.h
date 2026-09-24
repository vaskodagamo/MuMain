#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "ScriptMap.h"

#include <string>

namespace Editor::MapScript
{
// Turns the model and texture names of a parsed script into the loaded map's model
// types and texture slots. A model must be loaded on this map (its catalog name, its
// model name or its type, any case); a texture slot must hold a texture (its slot name
// such as "TileGround01", the file the map loaded into it, or its number). False with
// the op's field and what the map has instead when one is not there.
bool ResolveNames(EditScript& script, const MapContext& context, std::string& error);

// What reports call a model: the catalog's name, else the model's, else "type N".
std::string ModelDisplayName(const MapContext& context, int type);
} // namespace Editor::MapScript

#endif // _EDITOR
