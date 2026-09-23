#pragma once

#ifdef _EDITOR

#include "AssetCatalog.h"
#include "RegenRequest.h"

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

// The request folders on disk: <repo>/assets-work/<domain>/requests/<id>/ with
// request.json, brief.md and captures/. Files only; no engine or UI.
namespace Editor::Assets
{
std::filesystem::path RequestsDir(const std::filesystem::path& repoRoot, const RequestDomain& domain);

// Names of the folders under RequestsDir (every filed request's id).
std::vector<std::string> ExistingRequestIds(const std::filesystem::path& repoRoot, const RequestDomain& domain);

// Model names a new variant must not use, in lower case: every catalog model,
// every *.bmd in the domain's model folders of the repository's game data
// (Object{N} for a world), and the new_model of every other request.
std::set<std::string> TakenModelNames(const std::filesystem::path& repoRoot, const RequestDomain& domain,
                                      const Catalog& catalog);

// One file of a new request folder: where it goes inside the folder
// ("captures/01-front.jpg", '/' between folders) and its bytes.
struct RequestFile
{
    std::string relativePath;
    std::string bytes;
};

// Creates `folder`, which must not exist yet, with `files` in it. On failure
// nothing is left behind (the folder is removed again) and `error` says why.
bool WriteNewRequestFolder(const std::filesystem::path& folder, const std::vector<RequestFile>& files,
                           std::string& error);

// Writes request.json, brief.md and, when `jpeg` is not empty, the draft's first
// capture into a new folder RequestsDir(draft.domain)/<draft.id>. Refuses to touch a folder
// that exists already. On failure nothing is left behind and `error` says why.
bool WriteRequestFolder(const std::filesystem::path& repoRoot, const RequestDraft& draft,
                        const std::vector<std::uint8_t>& jpeg, std::filesystem::path& folder, std::string& error);
} // namespace Editor::Assets

#endif // _EDITOR
