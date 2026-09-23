#pragma once

#ifdef _EDITOR

#include "ItemCatalog.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The files the Item Editor's A/B compare can show an item with, besides the
// client's own: a Codex delivery of a request, a variant of the style pilot, or a
// folder the owner picks. Files only; unit-tested in
// tests/editor/test_item_candidates.cpp.
//
// A candidate is a folder with game files named like the item's (Axe01.bmd,
// Axes02.OZJ, ...). It needs to hold only what it replaces: the item is loaded
// with the candidate's model file when the folder has one, else the current one,
// and each texture from the candidate folder when it has that file, else from
// where the current texture is.
namespace Editor::Assets
{
enum class CandidateSource
{
    Delivery,       // assets-work/Items/requests/<id>/delivery/<key>/exports/
    DeliveryBefore, // the same delivery's original/: the files before the request
    Pilot,          // assets-work/Items/pilot/<Model>/<variant>/
    Folder,         // picked by the owner
};

struct ItemCandidate
{
    std::string id;    // unique for the item: "delivery:<request>", "before:<request>", "pilot:<Model>/<A>", "folder:<path>"
    std::string label; // for the UI: "delivery <request>", "before <request>", "pilot A", "folder <name>"
    CandidateSource source = CandidateSource::Folder;
    std::filesystem::path folder;
    std::string requestId; // deliveries: the request folder's name

    bool operator==(const ItemCandidate&) const = default;
};

// The candidates on disk for `item` under <repoRoot>/assets-work/Items: first each
// request's delivery for the item's key (and the delivery's "before" files), by
// request id, then each style pilot variant of the item's own model, by name. A
// folder counts only when it holds the item's model file or one of its textures.
std::vector<ItemCandidate> FindItemCandidates(const std::filesystem::path& repoRoot, const ItemCatalogEntry& item);

// `folder` as a candidate the owner picked; nullopt when it holds none of the
// item's files.
std::optional<ItemCandidate> CandidateFromFolder(const std::filesystem::path& folder, const ItemCatalogEntry& item);

// Where one model's files are for a variant: its BMD, and the folders its
// textures are looked for in (each texture from the first that holds it).
struct ModelFiles
{
    std::filesystem::path bmd;
    std::vector<std::filesystem::path> textureFolders;
};

// `model` inside a data tree laid out like src/bin/Data (the checkout's, the
// originals' or the game's own): <dataRoot>/Item/Sword01.bmd, textures from the
// folders of the catalog's texture files, then the model's own folder. nullopt
// when the catalog path is not under src/bin/Data.
std::optional<ModelFiles> ModelFilesIn(const std::filesystem::path& dataRoot, const ItemModel& model);

// `model` with `candidateFolder` over the files under `currentDataRoot`: the
// candidate's model file when the folder has it (any letter case), else the
// current one; textures from the candidate folder first.
std::optional<ModelFiles> CandidateModelFiles(const std::filesystem::path& currentDataRoot, const ItemModel& model,
                                              const std::filesystem::path& candidateFolder);

// The catalog texture files of `item` (e.g. src/bin/Data/Item/Axes02.OZJ) that
// `candidateFolder` has a replacement for, sorted.
std::vector<std::string> ReplacedTextures(const ItemCatalogEntry& item, const std::filesystem::path& candidateFolder);

// The other consumers (item keys, other:<MODEL_...>) of the texture files in
// `textures`, from the catalog's shared_with, without `item` itself; sorted.
std::vector<std::string> TexturePartners(const ItemCatalogEntry& item, const std::vector<std::string>& textures);

// True when every file the candidate holds for `item` has the same bytes as the
// file it replaces under `currentDataRoot` (the delivery is installed there
// already, e.g. the worker's branch is checked out). False when it holds none.
bool CandidateMatchesCurrent(const std::filesystem::path& currentDataRoot, const ItemCatalogEntry& item,
                             const std::filesystem::path& candidateFolder);

// "MODEL_HELM_MONK + 1" -> {"MODEL_HELM_MONK", 1}, "MODEL_MASK_HELM" -> {.., 0};
// nullopt for anything else.
std::optional<std::pair<std::string, int>> ParseModelConstant(const std::string& text);
} // namespace Editor::Assets

#endif // _EDITOR
