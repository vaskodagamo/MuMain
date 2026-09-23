#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The versions of the art the editors' A/B compare switches between: the
// checkout's current files (<repo>/src/bin/Data) and the originals from before
// the art rebuild, which tools/world_editor/materialize_variant.py writes from git
// into <repo>/out/ab/original/Data with the same layout and a manifest.json (world
// runs) or items-manifest.json (--items runs). The Item Editor also shows what the
// build copied next to the game (as built) and candidates (a Codex delivery, a
// pilot variant, any folder).
// File system only; unit-tested in tests/editor/test_model_preflight.cpp.
namespace Editor::Assets
{
enum class AssetVariant
{
    Current,
    Original,
    AsBuilt,   // the game's own Data folder, which the last build copied from src/bin/Data
    Candidate, // files from outside the data tree: a delivery, a pilot variant, a folder
};

// "current", "original", "as built" or "candidate", as the UI and the script name them.
const char* VariantName(AssetVariant variant);

// <repo>/src/bin/Data or <repo>/out/ab/original/Data; empty for the variants that
// are not in the repository (as built, candidate).
std::filesystem::path VariantDataRoot(const std::filesystem::path& repoRoot, AssetVariant variant);

// A catalog path such as src/bin/Data/Object1/Tree01.bmd inside the variant
// (<root>/Object1/Tree01.bmd); nullopt when the path is not under src/bin/Data.
std::optional<std::filesystem::path> VariantFile(const std::filesystem::path& repoRoot, AssetVariant variant,
                                                 const std::string& catalogPath);

// The command that builds `variant` for `world`. It names the script by its full
// path in `repoRoot`, so it runs from any folder, in the platform's usual shell:
// python3 on macOS and Linux, the py launcher on Windows (python.org installs it).
std::string MaterializeCommand(const std::filesystem::path& repoRoot, AssetVariant variant, int world);

enum class VariantState
{
    Ready,     // the current files, or a variant built from the catalog.json on disk now
    Missing,   // never built, or built for another world
    OutOfDate, // built from an older catalog.json: run the command again
};

// Reads the variant's manifest.json and compares its catalog SHA-256 with the
// world's catalog.json. Reads and hashes files: call it on demand, not every frame.
VariantState CheckVariant(const std::filesystem::path& repoRoot, AssetVariant variant, int world);

// The command that builds the item originals (materialize_variant.py original --items).
std::string MaterializeItemsCommand(const std::filesystem::path& repoRoot);

// Whether out/ab/original holds the item originals: Missing without its
// items-manifest.json, OutOfDate when a model of `originals` (catalog BMD path ->
// the catalog's original SHA-256) is not in it with that SHA-256. Reads the
// manifest: call it on demand, not every frame.
VariantState CheckItemOriginals(const std::filesystem::path& repoRoot,
                                const std::vector<std::pair<std::string, std::string>>& originals);
} // namespace Editor::Assets

#endif // _EDITOR
