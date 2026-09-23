#pragma once

#ifdef _EDITOR

#include "ItemRequest.h"

#include <string>

namespace Editor::Assets
{
// brief.md of an item request folder: request.json written out as prose for the
// owner and the worker, with the owner's notes, every target's model files and
// textures, the scope, the must-keep rules and the captures (links relative to the
// request folder).
std::string BuildItemBrief(const ItemRequestDraft& draft);
} // namespace Editor::Assets

#endif // _EDITOR
