#pragma once

#ifdef _EDITOR

#include "ItemAbCaptures.h"
#include "ItemPreview.h"

#include "Assets/AssetVariant.h"
#include "Assets/ItemCandidates.h"
#include "Assets/ItemCatalog.h"
#include "Core/ModelCopy.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// The A/B compare in the Item Editor's Browse details: which files the client
// shows an item with - as built (what the build copied next to the game), current
// (the checkout's src/bin/Data), original (from before the art rebuild, built by
// materialize_variant.py original --items) or a candidate (a Codex delivery, a
// style pilot variant, a folder) - for one item, its family or every item, and a
// second picture of the item from other files next to the preview (side by side,
// one camera). Switching goes through the hot reload (Core/ModelHotReload), the
// right picture through a model copy (Core/ModelCopy); neither changes a file.
class CItemAbCompare
{
public:
    static CItemAbCompare& GetInstance();

    // Before the preview's Render(): asks it for the side-by-side view of `item`
    // when that is on.
    void PassCompareToPreview(const Editor::Assets::ItemCatalogEntry& item);

    // Under the preview: what the client shows, the variant choice, side by side,
    // family / all switches, captures.
    void Render(const Editor::Assets::ItemCatalogEntry& item, const Editor::Assets::ItemCatalog& catalog);

    // The Requests tab's Compare: shows the item of `itemKey` side by side with the
    // delivery of `requestId` next time its details are drawn.
    void OpenDeliveryCompare(const std::string& requestId, const std::string& itemKey);

    // True while the side-by-side view is on (Browse widens the details for it).
    bool IsSideBySide() const { return m_sideBySide; }

    // Frees the model copies of the right picture (the editor shuts down).
    void Release();

private:
    // One of the versions the client (left) or the right picture shows.
    struct Choice
    {
        Editor::Assets::AssetVariant variant = Editor::Assets::AssetVariant::Current;
        std::string candidateId; // AssetVariant::Candidate only

        bool operator==(const Choice&) const = default;
    };

    struct Batch
    {
        std::string what; // "Swords", "All items"
        Choice choice;
        int total = 0;
        int done = 0;
        int refused = 0;
        std::string firstRefusal;
        std::size_t texturesBefore = 0;
        std::uint32_t textureBytesBefore = 0;
    };

    struct PendingDelivery
    {
        std::string requestId;
        std::string itemKey;
    };

    CItemAbCompare() = default;

    void EnsureSetup();
    void EnsureItem(const Editor::Assets::ItemCatalogEntry& item);
    void RescanCandidates(const Editor::Assets::ItemCatalogEntry& item);
    void CheckOriginals(const Editor::Assets::ItemCatalog& catalog);
    void ApplyPendingDelivery(const Editor::Assets::ItemCatalogEntry& item);
    void TakeOutcomes();
    void FinishBatchIfDone();

    // Files and requests.
    const Editor::Assets::ItemCandidate* FindCandidate(const std::string& id) const;
    std::string ChoiceLabel(const Choice& choice) const;
    std::optional<Editor::Assets::ModelFiles> FilesFor(const Editor::Assets::ItemModel& model, const Choice& choice) const;
    std::vector<Editor::Assets::HotReload::Request> RequestsFor(const Editor::Assets::ItemCatalogEntry& item,
                                                                  const Choice& choice, std::string& why) const;
    bool QueueItem(const Editor::Assets::ItemCatalogEntry& item, const Choice& choice);
    void QueueMany(const std::vector<const Editor::Assets::ItemCatalogEntry*>& items, const Choice& choice,
                   const std::string& what);
    void StartBatch(const std::string& what, const Choice& choice);

    // What the client shows.
    std::optional<Choice> ShownChoice(int type) const;
    std::string ShownLabel(const Editor::Assets::ItemCatalogEntry& item) const;
    bool IsBusy() const;

    // The right picture.
    void UpdateCopies(const Editor::Assets::ItemCatalogEntry& item);
    void DropCopies();

    // UI.
    void RenderShown(const Editor::Assets::ItemCatalogEntry& item);
    void RenderClientChoice(const Editor::Assets::ItemCatalogEntry& item);
    void RenderCandidateChoice(const Editor::Assets::ItemCatalogEntry& item, const std::optional<Choice>& shown);
    void RenderSideBySide();
    void RenderRightCombo();
    void RenderSharedTextures(const Editor::Assets::ItemCatalogEntry& item, const Editor::Assets::ItemCatalog& catalog);
    void RenderManySwitch(const Editor::Assets::ItemCatalogEntry& item, const Editor::Assets::ItemCatalog& catalog);
    void RenderTools(const Editor::Assets::ItemCatalogEntry& item);
    void RenderOriginalsHint(const Editor::Assets::ItemCatalog& catalog);
    void PollFolderPick(const Editor::Assets::ItemCatalogEntry& item);

    bool m_setUp = false;
    std::filesystem::path m_repo;
    std::filesystem::path m_currentRoot;  // <repo>/src/bin/Data
    std::filesystem::path m_originalRoot; // <repo>/out/ab/original/Data
    std::filesystem::path m_builtRoot;    // the game's own Data folder (absolute)
    Editor::Assets::VariantState m_originals = Editor::Assets::VariantState::Missing;
    bool m_originalsChecked = false;
    std::string m_materializeCommand;

    // The selected item and its candidates (found on disk, and folders the owner
    // picked, by item key).
    std::string m_itemKey;
    std::vector<int> m_itemTypes; // its models' Models[] slots
    std::vector<Editor::Assets::ItemCandidate> m_candidates;
    std::map<std::string, std::vector<Editor::Assets::ItemCandidate>> m_pickedFolders;
    std::optional<PendingDelivery> m_pendingDelivery;

    // The candidate each model type was last switched to (by id), for its label.
    std::unordered_map<int, std::string> m_candidateOfType;
    std::unordered_map<int, std::string> m_queuedCandidate;

    // Side by side.
    bool m_sideBySide = false;
    Choice m_right{Editor::Assets::AssetVariant::Original, ""};
    std::string m_copiesKey; // item key + right choice the copies were loaded for
    std::vector<std::unique_ptr<Editor::Assets::HotReload::ModelCopy>> m_copies;
    std::uint64_t m_copiesGeneration = 0;
    std::string m_copyMessage;
    std::string m_note; // e.g. the delivery is installed already

    std::optional<Batch> m_batch;
    std::string m_status;

    CItemAbCaptures m_captures;
};

#define g_ItemAbCompare CItemAbCompare::GetInstance()

#endif // _EDITOR
