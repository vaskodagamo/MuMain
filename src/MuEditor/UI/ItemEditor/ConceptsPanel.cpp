#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptsPanel.h"

#include "ConceptGenerateDialog.h"
#include "ConceptImageCache.h"
#include "ConceptJob.h"
#include "ConceptLibrary.h"
#include "ConceptRefineDialog.h"
#include "ConceptTool.h"
#include "ItemRequestDialog.h"

#include "Assets/ConceptCommands.h"
#include "Core/MuEditorCore.h"
#include "Render/Renderer/MuRenderer.h"
#include "UI/Console/MuEditorConsoleUI.h"

#include <algorithm>

using Editor::Concepts::ConceptBatchGroup;
using Editor::Concepts::ConceptListing;
using Editor::Concepts::ConceptVariant;
namespace Tool = Editor::ItemEditor::ConceptTool;

namespace
{
constexpr const char* VIEWER_TITLE = "Concept";
constexpr float THUMB_TARGET = 190.0f; // pixels at 100% UI scale; the columns fill the panel
constexpr int THUMB_PIXELS = 384;      // decoded size of a thumbnail
constexpr int FULL_PIXELS = 1536;      // decoded size in the big view (the images are at most this)
constexpr float VIEWER_SHARE = 0.85f;  // of the window
constexpr float MIN_ZOOM = 1.0f;       // 1 = the whole image fits
constexpr float MAX_ZOOM = 4.0f;
constexpr float BORDER_THICKNESS = 2.0f;
// While a job writes this item's variants, read the list again at most this often.
constexpr std::chrono::milliseconds LIVE_RELOAD_INTERVAL(1000);

constexpr ImU32 BORDER_PLAIN = IM_COL32(90, 90, 100, 255);
constexpr ImU32 BORDER_PICKED = IM_COL32(90, 210, 110, 255);
constexpr ImU32 BORDER_DISCARDED = IM_COL32(120, 60, 60, 255);
constexpr ImU32 IMAGE_WAITING = IM_COL32(28, 28, 32, 255);
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 PICKED_COLOR{0.45f, 0.85f, 0.45f, 1.0f};
constexpr ImVec4 ERROR_COLOR{1.0f, 0.45f, 0.4f, 1.0f};

float Scaled(float pixels)
{
    return pixels * g_MuEditorCore.GetUIScale();
}

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"%hs\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor(message);
}

// The texture of `path` for ImGui, or null while it loads; `aspect` is width / height.
void* TextureOf(const std::string& path, int pixels, float& aspect)
{
    const CConceptImageCache::Image& image = g_ConceptImages.Get(path, pixels);
    aspect = image.height > 0 ? static_cast<float>(image.width) / static_cast<float>(image.height) : 1.0f;
    return image.texture != 0 ? mu::GetRenderer().GetTexturePointer(image.texture) : nullptr;
}

// `size` laid out at `corner` with the image fitted and centred in it.
void DrawFitted(ImDrawList* drawList, void* texture, float aspect, const ImVec2& corner, const ImVec2& size)
{
    ImVec2 picture = size;
    if (aspect >= size.x / size.y)
        picture.y = size.x / aspect;
    else
        picture.x = size.y * aspect;
    const ImVec2 min(corner.x + (size.x - picture.x) * 0.5f, corner.y + (size.y - picture.y) * 0.5f);
    drawList->AddImage((ImTextureID)(intptr_t)texture, min, ImVec2(min.x + picture.x, min.y + picture.y));
}
} // namespace

CConceptsPanel& CConceptsPanel::GetInstance()
{
    static CConceptsPanel instance;
    return instance;
}

void CConceptsPanel::Load(const std::string& key)
{
    m_loadingKey = key;
    m_reloadWanted = false;
    m_lastLoad = std::chrono::steady_clock::now();
    m_load = Tool::RunJson(Editor::Concepts::ListKeyArgs(key, Tool::Repo()));
}

void CConceptsPanel::PollLoad()
{
    if (!Tool::IsReady(m_load))
        return;
    const Tool::JsonReply reply = Tool::ReplyOf(m_load.get());
    if (m_loadingKey != m_key)
        return; // another item was selected meanwhile; its list is asked for next
    ConceptListing listing;
    m_loadProblem = reply.problem;
    if (reply.ok && !Editor::Concepts::ParseConceptListing(reply.text, listing))
        m_loadProblem = "concepts.py list: unreadable answer";
    else if (reply.ok && !listing.ok)
        m_loadProblem = "concepts.py list: " + listing.error;
    if (m_loadProblem.empty())
        m_listing = std::move(listing);
}

void CConceptsPanel::RefreshAfterJobs()
{
    if (m_jobVersionSeen == g_ConceptJob.Version())
        return;
    const bool throttled = g_ConceptJob.IsRunning() &&
                           std::chrono::steady_clock::now() - m_lastLoad < LIVE_RELOAD_INTERVAL;
    if (throttled)
        return;
    m_jobVersionSeen = g_ConceptJob.Version();
    m_reloadWanted = true;
}

void CConceptsPanel::RunAction(std::vector<std::string> arguments, std::string label, FollowUp followUp)
{
    if (m_action.valid())
        return;
    m_actionLabel = std::move(label);
    m_followUp = followUp;
    m_actionError.clear();
    m_action = Tool::RunJson(std::move(arguments));
}

void CConceptsPanel::PollAction()
{
    if (!Tool::IsReady(m_action))
        return;
    const Tool::JsonReply reply = Tool::ReplyOf(m_action.get());
    std::string error = reply.problem;
    const bool ok = reply.ok && Editor::Concepts::CommandSucceeded(reply.text, error);
    Log("[Concepts] " + m_actionLabel + (ok ? ": done" : ": failed - " + error));
    if (!ok)
        m_actionError = m_actionLabel + " failed: " + error;
    if (m_listing && m_listing->pick)
        g_ConceptImages.Forget(m_listing->pick->image); // a pick replaces concept.jpg
    g_ConceptLibrary.Refresh();
    m_reloadWanted = true;
    if (ok && m_followUp == FollowUp::AskCodex)
        OpenAskCodex();
    m_followUp = FollowUp::None;
}

void CConceptsPanel::OpenAskCodex()
{
    if (m_catalog == nullptr)
        return;
    // The concept of an armour set is filed under its body armour: ask for that part.
    const std::string conceptKey = m_listing ? m_listing->key : m_key;
    const Editor::Assets::ItemCatalogEntry* entry = m_catalog->FindByKey(conceptKey);
    if (entry == nullptr)
        entry = m_catalog->FindByKey(m_key);
    if (entry != nullptr)
        g_ItemRequestDialog.Open(*entry, *m_catalog);
}

void CConceptsPanel::Render(const Editor::Assets::ItemCatalogEntry& item, const Editor::Assets::ItemCatalog& catalog)
{
    m_catalog = &catalog;
    if (Tool::Repo().empty())
        return; // RenderOwnerActions says a checkout is needed
    if (item.key != m_key)
    {
        m_key = item.key;
        m_itemName = item.name.empty() ? item.key : item.name;
        m_listing.reset();
        m_loadProblem.clear();
        m_actionError.clear();
        m_reloadWanted = true;
    }
    RefreshAfterJobs();
    PollLoad();
    PollAction();
    if (m_reloadWanted && !m_load.valid())
        Load(m_key);

    if (!ImGui::CollapsingHeader("Concepts", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    RenderHeader(item);
    if (!m_listing)
        return;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float available = ImGui::GetContentRegionAvail().x;
    const int columns = std::max(1, static_cast<int>((available + spacing) / (Scaled(THUMB_TARGET) + spacing)));
    const float side = (available - spacing * (columns - 1)) / columns;
    RenderReference(side);
    if (columns > 1)
        ImGui::SameLine();
    RenderPick(side);
    RenderBatches(side);
}

void CConceptsPanel::RenderHeader(const Editor::Assets::ItemCatalogEntry& item)
{
    if (ImGui::Button("Generate concepts for this item..."))
        g_ConceptGenerateDialog.Open({{item.key, m_itemName}});
    if (m_listing)
    {
        const int discarded = static_cast<int>(std::count_if(m_listing->variants.begin(), m_listing->variants.end(),
                                                             [](const ConceptVariant& v) { return v.discarded; }));
        ImGui::SameLine();
        const std::string label = "Show discarded (" + std::to_string(discarded) + ")";
        ImGui::Checkbox(label.c_str(), &m_showDiscarded);
    }
    if (!m_listing && m_load.valid())
        ImGui::TextColored(NOTE_COLOR, "Reading the concepts (concepts.py list)...");
    if (!m_loadProblem.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", m_loadProblem.c_str());
    if (!m_actionError.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", m_actionError.c_str());
    if (m_action.valid())
        ImGui::TextColored(NOTE_COLOR, "%s...", m_actionLabel.c_str());
    if (m_listing)
    {
        for (const std::string& warning : m_listing->warnings)
            ImGui::TextColored(NOTE_COLOR, "%s", warning.c_str());
    }
}

bool CConceptsPanel::RenderImage(const std::string& path, float side, ImU32 border)
{
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    const ImVec2 size(side, side);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(corner, ImVec2(corner.x + side, corner.y + side), IMAGE_WAITING);
    float aspect = 1.0f;
    if (void* texture = path.empty() ? nullptr : TextureOf(path, THUMB_PIXELS, aspect))
        DrawFitted(drawList, texture, aspect, corner, size);
    drawList->AddRect(corner, ImVec2(corner.x + side, corner.y + side), border, 0.0f, 0, BORDER_THICKNESS);
    const bool clicked = ImGui::InvisibleButton("##image", size);
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return clicked;
}

void CConceptsPanel::RenderReference(float side)
{
    ImGui::BeginGroup();
    ImGui::PushID("reference");
    const std::string path = m_listing->referenceExists ? m_listing->referencePath : std::string();
    if (RenderImage(path, side, BORDER_PLAIN) && !path.empty())
    {
        m_viewed = Viewed{path, m_itemName + ": current look (reference render)", std::nullopt};
        m_viewerPending = true;
    }
    ImGui::TextUnformatted("Current look");
    if (!m_listing->referenceExists)
        ImGui::TextColored(NOTE_COLOR, "no reference render yet");
    ImGui::PopID();
    ImGui::EndGroup();
}

void CConceptsPanel::RenderPick(float side)
{
    ImGui::BeginGroup();
    ImGui::PushID("pick");
    if (!m_listing->pick)
    {
        ImGui::Dummy(ImVec2(side, 0.0f));
        ImGui::TextColored(NOTE_COLOR, "No concept picked yet.");
        ImGui::PopID();
        ImGui::EndGroup();
        return;
    }
    const Editor::Concepts::ConceptPick& pick = *m_listing->pick;
    if (RenderImage(pick.image, side, BORDER_PICKED))
    {
        m_viewed = Viewed{pick.image, m_itemName + ": picked concept", std::nullopt};
        m_viewerPending = true;
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + side);
    ImGui::TextColored(PICKED_COLOR, "Picked: %s (%s)", pick.variant.c_str(), pick.picked.c_str());
    ImGui::PopTextWrapPos();
    ImGui::BeginDisabled(m_action.valid());
    if (ImGui::SmallButton("Ask Codex with it..."))
        OpenAskCodex();
    ImGui::SameLine();
    if (ImGui::SmallButton("Unpick"))
        RunAction(Editor::Concepts::UnpickArgs(m_listing->key, Tool::Repo()), "Unpick " + m_listing->key);
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::EndGroup();
}

void CConceptsPanel::RenderBatches(float side)
{
    const std::vector<ConceptBatchGroup> groups = Editor::Concepts::GroupByBatch(*m_listing, m_showDiscarded);
    if (groups.empty())
    {
        ImGui::TextColored(NOTE_COLOR, "No concept images for this item yet.");
        return;
    }
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (side + spacing)));
    for (const ConceptBatchGroup& group : groups)
    {
        ImGui::SeparatorText(group.batch.c_str());
        if (group.kind == "refine")
            ImGui::TextColored(NOTE_COLOR, "refine round");
        for (std::size_t i = 0; i < group.variants.size(); ++i)
        {
            if (i % columns != 0)
                ImGui::SameLine();
            RenderVariant(group.variants[i], side);
        }
    }
}

void CConceptsPanel::RenderVariant(std::size_t index, float side)
{
    const ConceptVariant& variant = m_listing->variants[index];
    ImGui::PushID(static_cast<int>(index));
    ImGui::BeginGroup();
    const ImU32 border = variant.picked ? BORDER_PICKED : variant.discarded ? BORDER_DISCARDED : BORDER_PLAIN;
    if (RenderImage(variant.image, side, border))
    {
        m_viewed = Viewed{variant.image, m_itemName + " - " + Editor::Concepts::VariantPath(variant.ref), index};
        m_viewerPending = true;
        m_zoom = MIN_ZOOM;
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + side);
    if (variant.picked)
        ImGui::TextColored(PICKED_COLOR, "%s - picked", variant.ref.variant.c_str());
    else
        ImGui::Text("%s%s", variant.ref.variant.c_str(), variant.discarded ? " (discarded)" : "");
    if (!variant.lineage.empty() || variant.parent)
        ImGui::TextColored(NOTE_COLOR, "from %s", Editor::Concepts::LineageText(variant).c_str());
    if (!variant.note.empty())
        ImGui::TextColored(NOTE_COLOR, "\"%s\"", variant.note.c_str());
    ImGui::PopTextWrapPos();
    RenderVariantActions(variant);
    ImGui::EndGroup();
    ImGui::PopID();
}

void CConceptsPanel::RenderVariantActions(const ConceptVariant& variant)
{
    ImGui::BeginDisabled(m_action.valid());
    if (variant.picked)
    {
        if (ImGui::SmallButton("Unpick"))
            RunAction(Editor::Concepts::UnpickArgs(m_listing->key, Tool::Repo()), "Unpick " + m_listing->key);
    }
    else if (ImGui::SmallButton("Pick"))
    {
        Pick(variant, FollowUp::None);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Refine..."))
        Refine(variant);
    ImGui::SameLine();
    if (ImGui::SmallButton(variant.discarded ? "Undiscard" : "Discard"))
        ToggleDiscarded(variant);
    ImGui::EndDisabled();
}

void CConceptsPanel::ToggleDiscarded(const ConceptVariant& variant)
{
    const bool discard = !variant.discarded;
    RunAction(Editor::Concepts::DiscardArgs(variant.ref, discard, Tool::Repo()),
              std::string(discard ? "Discard " : "Undiscard ") + Editor::Concepts::VariantPath(variant.ref));
}

void CConceptsPanel::Pick(const ConceptVariant& variant, FollowUp followUp)
{
    const std::string label = "Pick " + Editor::Concepts::VariantPath(variant.ref);
    RunAction(Editor::Concepts::PickArgs(variant.ref, Tool::Repo()), label, followUp);
}

void CConceptsPanel::AskCodexWith(const ConceptVariant& variant)
{
    if (variant.picked)
        OpenAskCodex();
    else
        Pick(variant, FollowUp::AskCodex);
}

void CConceptsPanel::Refine(const ConceptVariant& variant)
{
    g_ConceptRefineDialog.Open(variant.ref, variant.image, m_itemName);
}

void CConceptsPanel::RenderPopups()
{
    if (m_viewerPending)
    {
        ImGui::OpenPopup(VIEWER_TITLE);
        m_viewerPending = false;
    }
    RenderViewer();
}

void CConceptsPanel::RenderViewer()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(display.x * VIEWER_SHARE, display.y * VIEWER_SHARE), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal(VIEWER_TITLE, nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;
    g_MuEditorCore.SetHoveringUI(true);
    if (!m_viewed)
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::TextUnformatted(m_viewed->title.c_str());
    RenderViewerActions();
    if (!m_viewed)
    {
        ImGui::EndPopup(); // closed by one of the actions
        return;
    }
    ImGui::SetNextItemWidth(Scaled(THUMB_TARGET));
    ImGui::SliderFloat("Zoom", &m_zoom, MIN_ZOOM, MAX_ZOOM, "%.1fx");
    ImGui::SameLine();
    if (ImGui::Button("Fit"))
        m_zoom = MIN_ZOOM;
    ImGui::BeginChild("ConceptViewerImage", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    float aspect = 1.0f;
    void* texture = TextureOf(m_viewed->image, FULL_PIXELS, aspect);
    const ImVec2 area = ImGui::GetContentRegionAvail();
    const float fitHeight = std::min(area.y, area.x / aspect);
    const ImVec2 size(fitHeight * aspect * m_zoom, fitHeight * m_zoom);
    if (size.x < area.x)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (area.x - size.x) * 0.5f); // centred while it fits
    if (texture != nullptr)
        ImGui::Image((ImTextureID)(intptr_t)texture, size);
    else
        ImGui::TextColored(NOTE_COLOR, "Loading the image...");
    ImGui::EndChild();
    ImGui::EndPopup();
}

void CConceptsPanel::RenderViewerActions()
{
    const bool isVariant = m_viewed->variant && m_listing && *m_viewed->variant < m_listing->variants.size();
    if (isVariant)
    {
        const ConceptVariant variant = m_listing->variants[*m_viewed->variant];
        ImGui::BeginDisabled(m_action.valid());
        if (!variant.picked && ImGui::Button("Pick"))
            Pick(variant, FollowUp::None);
        if (!variant.picked)
            ImGui::SameLine();
        bool closing = false;
        if (ImGui::Button("Ask Codex with this concept..."))
        {
            AskCodexWith(variant);
            closing = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Refine with comment..."))
        {
            Refine(variant);
            closing = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(variant.discarded ? "Undiscard" : "Discard"))
        {
            ToggleDiscarded(variant);
            closing = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (closing)
            m_viewed.reset();
    }
    if (ImGui::Button("Close"))
        m_viewed.reset();
    if (!m_viewed)
        ImGui::CloseCurrentPopup();
}

#endif // _EDITOR
