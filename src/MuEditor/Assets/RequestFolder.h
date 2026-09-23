#pragma once

#ifdef _EDITOR

#include "AssetCatalog.h"
#include "RegenRequest.h"

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

// The request folders on disk: <repo>/assets-work/World{N}/requests/<id>/ with
// request.json, brief.md and captures/. Files only; no engine or UI.
namespace Editor::Assets
{
std::filesystem::path RequestsDir(const std::filesystem::path& repoRoot, int world);

// Names of the folders under RequestsDir (every filed request's id).
std::vector<std::string> ExistingRequestIds(const std::filesystem::path& repoRoot, int world);

// Model names a new variant must not use, in lower case: every catalog model,
// every Object{N}/*.bmd in the repository's game data, and the new_model of every
// other request.
std::set<std::string> TakenModelNames(const std::filesystem::path& repoRoot, int world, const Catalog& catalog);

// Writes request.json, brief.md and, when `jpeg` is not empty, the draft's first
// capture into a new folder RequestsDir/<draft.id>. Refuses to touch a folder
// that exists already. On failure nothing is left behind and `error` says why.
bool WriteRequestFolder(const std::filesystem::path& repoRoot, const RequestDraft& draft,
                        const std::vector<std::uint8_t>& jpeg, std::filesystem::path& folder, std::string& error);
} // namespace Editor::Assets

#endif // _EDITOR
