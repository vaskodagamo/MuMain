#pragma once

#ifdef _EDITOR

#include "AssetCatalog.h"

#include <filesystem>
#include <map>
#include <string>

// The owner's quick verdicts from looking at models in the client, kept in
// <repo>/assets-work/World{N}/client-review.json:
//
//   { "Tree01": { "verdict": "looks-good", "note": "", "date": "2026-09-23" }, ... }
//
// The owner owns this file (the editor writes it for them); the catalog builder
// folds it into catalog.json as client_verified and client_review.
namespace Editor::Assets
{
constexpr const char* VERDICT_LOOKS_GOOD = "looks-good";
constexpr const char* VERDICT_NEEDS_WORK = "needs-work";

using ClientReviews = std::map<std::string, ClientReview>;

std::filesystem::path ClientReviewFile(const std::filesystem::path& repoRoot, int world);

// Parses client-review.json text; entries that are not objects are skipped.
bool ParseClientReviews(const std::string& text, ClientReviews& out, std::string& error);

// The verdicts in `file`; an empty map (and no error) when the file does not exist.
ClientReviews ReadClientReviews(const std::filesystem::path& file, std::string& error);

// Sets the verdict of `model` in `file` (created when missing), keeping every
// other model's entry. Writes sorted keys with a two-space indent, so the file
// diffs cleanly. Returns false with `error` when the file cannot be read or written.
bool RecordClientReview(const std::filesystem::path& file, const std::string& model, const ClientReview& review,
                        std::string& error);
} // namespace Editor::Assets

#endif // _EDITOR
