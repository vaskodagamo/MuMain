#pragma once

#ifdef _EDITOR

#include "Assets/ConceptListing.h"
#include "Assets/ItemCatalog.h"
#include "Core/PythonTool.h"

#include "imgui.h"

#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

// The Concepts section of the Browse details: the selected item's current look
// (the reference render), its picked concept and every concept variant, batch by
// batch (newest first, a refine with the variant it came from), read with
// `concepts.py list --key K --json`. A click shows a variant big (fit, zoom). Per
// variant: Pick / Unpick, Refine with comment..., Discard / Undiscard (discarded
// ones are hidden unless shown), and "Ask Codex with this concept..." (picks it
// first when needed, then opens the Ask Codex dialog, which attaches the pick).
// The list is read again when the item changes, after a concepts job and after
// every action.
class CConceptsPanel
{
public:
    static CConceptsPanel& GetInstance();

    // Draws the section for `item`; `catalog` gives Ask Codex its set parts.
    void Render(const Editor::Assets::ItemCatalogEntry& item, const Editor::Assets::ItemCatalog& catalog);
    // The big view and the actions' follow-ups; call once per frame inside the Item Editor window.
    void RenderPopups();

private:
    enum class FollowUp
    {
        None,
        AskCodex, // open Ask Codex once the pick is written
    };

    // What the big view shows.
    struct Viewed
    {
        std::string image;
        std::string title;
        std::optional<std::size_t> variant; // index into the listing; none for the reference or the pick
    };

    CConceptsPanel() = default;

    void Load(const std::string& key);
    void PollLoad();
    void RefreshAfterJobs();
    void RunAction(std::vector<std::string> arguments, std::string label, FollowUp followUp = FollowUp::None);
    void PollAction();
    void OpenAskCodex();

    void RenderHeader(const Editor::Assets::ItemCatalogEntry& item);
    void RenderReference(float side);
    void RenderPick(float side);
    void RenderBatches(float side);
    void RenderVariant(std::size_t index, float side);
    void RenderVariantActions(const Editor::Concepts::ConceptVariant& variant);
    bool RenderImage(const std::string& path, float side, ImU32 border);
    void RenderViewer();
    void RenderViewerActions();

    void Pick(const Editor::Concepts::ConceptVariant& variant, FollowUp followUp);
    void AskCodexWith(const Editor::Concepts::ConceptVariant& variant);
    void Refine(const Editor::Concepts::ConceptVariant& variant);
    void ToggleDiscarded(const Editor::Concepts::ConceptVariant& variant);

    // The item shown (Browse's key) and its listing.
    std::string m_key;
    std::string m_itemName;
    std::optional<Editor::Concepts::ConceptListing> m_listing;
    std::future<Editor::PythonTool::Result> m_load;
    std::string m_loadingKey;
    bool m_reloadWanted = false;
    std::uint64_t m_jobVersionSeen = 0;
    std::chrono::steady_clock::time_point m_lastLoad;
    std::string m_loadProblem;

    bool m_showDiscarded = false;

    std::future<Editor::PythonTool::Result> m_action;
    std::string m_actionLabel;
    FollowUp m_followUp = FollowUp::None;
    std::string m_actionError;
    const Editor::Assets::ItemCatalog* m_catalog = nullptr; // Browse's, set by Render()

    std::optional<Viewed> m_viewed;
    bool m_viewerPending = false;
    float m_zoom = 1.0f;
};

#define g_ConceptsPanel CConceptsPanel::GetInstance()

#endif // _EDITOR
