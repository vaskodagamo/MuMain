#pragma once

#ifdef _EDITOR

#include "RegenRequest.h"

#include <string>

namespace Editor::Assets
{
// brief.md of a request folder: request.json written out as prose for the owner
// and the worker, with links (relative to the request folder) to ASTRA.md, the
// request README, the batch notes of every target, the capture and the offline
// previews.
std::string BuildBrief(const RequestDraft& draft);
} // namespace Editor::Assets

#endif // _EDITOR
