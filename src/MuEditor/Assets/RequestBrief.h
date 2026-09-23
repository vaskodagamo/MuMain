#pragma once

#ifdef _EDITOR

#include "RegenRequest.h"

#include <sstream>
#include <string>
#include <vector>

// Markdown pieces every brief.md uses (the world briefs here, the item briefs in
// ItemRequestBrief.h).
namespace Editor::Assets::BriefText
{
// A link from brief.md (assets-work/<domain>/requests/<id>/) to a repository path.
std::string RepoLink(const std::string& repoPath);
// `text` as inline code.
std::string Code(const std::string& text);
// One "- item" line per entry, or "- (none)".
void BulletList(std::ostringstream& out, const std::vector<std::string>& items);
} // namespace Editor::Assets::BriefText

namespace Editor::Assets
{
// brief.md of a request folder: request.json written out as prose for the owner
// and the worker, with links (relative to the request folder) to ASTRA.md, the
// request README, the batch notes of every target, the capture and the offline
// previews.
std::string BuildBrief(const RequestDraft& draft);
} // namespace Editor::Assets

#endif // _EDITOR
