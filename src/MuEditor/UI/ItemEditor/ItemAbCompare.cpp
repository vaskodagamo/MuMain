#include "stdafx.h"

#ifdef _EDITOR

#include "ItemAbCompare.h"

#include "ItemModelTypes.h"

#include "Assets/EditorText.h"
#include "Core/EditorFiles.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "UI/MapEditor/MapEditorFilePicker.h"
#include "UI/MapEditor/MapEditorStatusLine.h"

#include "Render/Sprites/GlobalBitmap.h" // Bitmaps: texture count and memory

#include "imgui.h"

#include <algorithm>
#include <cstdio>

using namespace Editor::Assets;
namespace HotReload = Editor::Assets::HotReload;
namespace fs = std::filesystem;
using Editor::Files::FilePickRequest;

namespace
{
constexpr const char* PANEL_TITLE = "A/B compare";
constexpr double BYTES_PER_MB = 1024.0 * 1024.0;
constexpr std::size_t MAX_PARTNERS_LISTED = 8;
constexpr const char* LEFT_PREFIX = "client: ";
constexpr std::string_view COPIES_KEY_SEPARATOR = "|";

constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 ORIGINAL_COLOR{0.55f, 0.8f, 1.0f, 1.0f};
constexpr ImVec4 CANDIDATE_COLOR{0.8f, 0.6f, 1.0f, 1.0f};

// The versions every item has, in the order the buttons show them.
constexpr AssetVariant BASE_VARIANTS[] = {AssetVariant::AsBuilt, AssetVariant::Current, AssetVariant::Original};

// Keeps the next button of a row on this line when it fits, else starts a new line.
void SameLineIfFits(float width)
{
    const float lineEnd = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width;
    if (lineEnd <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
        ImGui::SameLine();
}

float RadioWidth(const char* label)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
}

void HoverTip(const char* text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", text);
}

const char* VariantTip(AssetVariant variant)
{
    switch (variant)
    {
    case AssetVariant::AsBuilt:
        return "The files the last build copied next to the game (its Data folder): what the client loaded at start.";
    case AssetVariant::Current:
        return "The checkout's src/bin/Data: pulled or delivered files show here before the next build.";
    case AssetVariant::Original:
        return "The files from before the art rebuild (out/ab/original, built by materialize_variant.py original "
               "--items).";
    case AssetVariant::Candidate:
        break;
    }
    return "Files from outside the data tree: a Codex delivery, a style pilot variant or a folder you picked. What it "
           "does not hold comes from src/bin/Data.";
}

std::string TextureLine()
{
    char text[96];
    std::snprintf(text, sizeof(text), "%zu textures, %.1f MB", Bitmaps.GetNumberOfTexture(),
                  Bitmaps.GetUsedTextureMemory() / BYTES_PER_MB);
    return text;
}

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"[Items] %hs\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor("[Items] " + message);
}

std::string PartnerName(const std::string& partner, const ItemCatalog& catalog)
{
    const ItemCatalogEntry* item = catalog.FindByKey(partner);
    return item != nullptr && !item->name.empty() ? partner + " " + item->name : partner;
}

std::string PartnerList(const std::vector<std::string>& partners, const ItemCatalog& catalog)
{
    std::vector<std::string> names;
    for (std::size_t i = 0; i < partners.size() && i < MAX_PARTNERS_LISTED; ++i)
        names.push_back(PartnerName(partners[i], catalog));
    std::string text = Editor::Text::Join(names, ", ");
    if (partners.size() > MAX_PARTNERS_LISTED)
        text += " and " + std::to_string(partners.size() - MAX_PARTNERS_LISTED) + " more";
    return text;
}

// Every texture file of the item that another item or model uses too.
std::vector<std::string> SharedTextureFiles(const ItemCatalogEntry& item)
{
    std::vector<std::string> files;
    for (const auto& [container, partners] : item.sharedWith)
        files.push_back(container);
    return files;
}
} // namespace

CItemAbCompare& CItemAbCompare::GetInstance()
{
    static CItemAbCompare instance;
    return instance;
}

void CItemAbCompare::EnsureSetup()
{
    if (m_setUp)
        return;
    m_setUp = true;
    Editor::ItemEditor::AllowItemReloads();
    m_repo = Editor::Files::RepoRoot().root;
    m_builtRoot = Editor::Files::AbsolutePath(Editor::Files::DataDir());
    if (m_repo.empty())
        return;
    m_currentRoot = VariantDataRoot(m_repo, AssetVariant::Current);
    m_originalRoot = VariantDataRoot(m_repo, AssetVariant::Original);
    m_materializeCommand = MaterializeItemsCommand(m_repo);
}

void CItemAbCompare::EnsureItem(const ItemCatalogEntry& item)
{
    if (item.key == m_itemKey)
        return;
    m_itemKey = item.key;
    m_itemTypes.clear();
    for (const ItemModel& model : item.models)
    {
        if (const std::optional<int> type = Editor::ItemEditor::ModelTypeOf(item, model))
            m_itemTypes.push_back(*type);
    }
    m_note.clear();
    m_copyMessage.clear();
    RescanCandidates(item);
    // A new item compares with its first candidate; without one, with the version
    // chosen for the item before (a candidate of that item: its original).
    if (!m_candidates.empty())
        m_right = {AssetVariant::Candidate, m_candidates.front().id};
    else if (m_right.variant == AssetVariant::Candidate)
        m_right = {AssetVariant::Original, ""};
}

void CItemAbCompare::RescanCandidates(const ItemCatalogEntry& item)
{
    m_candidates = FindItemCandidates(m_repo, item);
    const auto picked = m_pickedFolders.find(item.key);
    if (picked != m_pickedFolders.end())
        m_candidates.insert(m_candidates.end(), picked->second.begin(), picked->second.end());
}

void CItemAbCompare::CheckOriginals(const ItemCatalog& catalog)
{
    m_originalsChecked = true;
    std::vector<std::pair<std::string, std::string>> originals;
    for (const ItemCatalogEntry& item : catalog.items)
    {
        for (const ItemModel& model : item.models)
            originals.emplace_back(model.bmd, model.originalSha256);
    }
    m_originals = m_repo.empty() ? VariantState::Missing : CheckItemOriginals(m_repo, originals);
}

void CItemAbCompare::OpenDeliveryCompare(const std::string& requestId, const std::string& itemKey)
{
    m_pendingDelivery = PendingDelivery{requestId, itemKey};
}

// Right: the delivery. Left: the checkout's current files, or, when they already
// are the delivery (the worker's branch is checked out), what the delivery replaced.
void CItemAbCompare::ApplyPendingDelivery(const ItemCatalogEntry& item)
{
    if (!m_pendingDelivery || m_pendingDelivery->itemKey != item.key)
        return;
    const std::string requestId = m_pendingDelivery->requestId;
    m_pendingDelivery.reset();
    RescanCandidates(item);
    const ItemCandidate* delivery = FindCandidate("delivery:" + requestId);
    if (delivery == nullptr)
    {
        m_status = "Request " + requestId + " has no files for " + item.key + " under delivery/" + item.key +
                   "/exports/.";
        return;
    }
    m_sideBySide = true;
    m_right = {AssetVariant::Candidate, delivery->id};
    m_copiesKey.clear();
    Choice left{AssetVariant::Current, ""};
    m_note.clear();
    if (CandidateMatchesCurrent(m_currentRoot, item, delivery->folder))
    {
        const ItemCandidate* before = FindCandidate("before:" + requestId);
        left = before != nullptr ? Choice{AssetVariant::Candidate, before->id} : Choice{AssetVariant::Original, ""};
        m_note = "src/bin/Data already holds this delivery (the worker's branch is checked out), so the left picture "
                 "shows " + ChoiceLabel(left) + " instead of current.";
    }
    QueueItem(item, left);
}

void CItemAbCompare::TakeOutcomes()
{
    bool shownItemChanged = false;
    for (const HotReload::ModelRange& range : Editor::ItemEditor::ItemRanges())
    {
        for (const HotReload::Outcome& outcome : HotReload::TakeOutcomes(range))
        {
            const int type = outcome.request.type;
            if (outcome.loaded && outcome.request.variant == AssetVariant::Candidate)
                m_candidateOfType[type] = m_queuedCandidate[type];
            else if (outcome.loaded)
                m_candidateOfType.erase(type);
            shownItemChanged |= outcome.loaded && std::find(m_itemTypes.begin(), m_itemTypes.end(), type) != m_itemTypes.end();
            m_status = outcome.message;
            if (!m_batch)
                continue;
            ++m_batch->done;
            if (!outcome.loaded && m_batch->refused++ == 0)
                m_batch->firstRefusal = outcome.message;
        }
    }
    // The preview framed the item and dressed its character with the old model.
    if (shownItemChanged)
        g_ItemPreview.ModelsChanged();
    FinishBatchIfDone();
}

void CItemAbCompare::FinishBatchIfDone()
{
    if (!m_batch || IsBusy() || m_batch->done < m_batch->total)
        return;
    const Batch batch = *m_batch;
    m_batch.reset();
    char textures[160];
    std::snprintf(textures, sizeof(textures), "Textures: %zu -> %zu, %.1f MB -> %.1f MB.", batch.texturesBefore,
                  Bitmaps.GetNumberOfTexture(), batch.textureBytesBefore / BYTES_PER_MB,
                  Bitmaps.GetUsedTextureMemory() / BYTES_PER_MB);
    std::string summary = batch.what + ": " + std::to_string(batch.done - batch.refused) + " model(s) now show " +
                          ChoiceLabel(batch.choice) + ". " + textures;
    if (batch.refused > 0)
        summary += " " + std::to_string(batch.refused) + " not changed (see the Editor Console), first: " +
                   batch.firstRefusal;
    if (batch.refused == batch.total)
        summary = batch.firstRefusal + (batch.total > 1 ? " (and every other model: see the Editor Console)" : "");
    Log(summary + " (" + std::to_string(Bitmaps.GetUsedTextureMemory()) + " bytes)");
    m_status = batch.total == 1 && batch.refused == 0 ? m_status + " " + textures : summary;
}

const ItemCandidate* CItemAbCompare::FindCandidate(const std::string& id) const
{
    const auto found = std::find_if(m_candidates.begin(), m_candidates.end(),
                                    [&](const ItemCandidate& candidate) { return candidate.id == id; });
    return found != m_candidates.end() ? &*found : nullptr;
}

std::string CItemAbCompare::ChoiceLabel(const Choice& choice) const
{
    if (choice.variant != AssetVariant::Candidate)
        return VariantName(choice.variant);
    const ItemCandidate* candidate = FindCandidate(choice.candidateId);
    return candidate != nullptr ? candidate->label : "a candidate";
}

std::optional<ModelFiles> CItemAbCompare::FilesFor(const ItemModel& model, const Choice& choice) const
{
    switch (choice.variant)
    {
    case AssetVariant::Current:
        return ModelFilesIn(m_currentRoot, model);
    case AssetVariant::Original:
        return ModelFilesIn(m_originalRoot, model);
    case AssetVariant::AsBuilt:
        return ModelFilesIn(m_builtRoot, model);
    case AssetVariant::Candidate:
        break;
    }
    const ItemCandidate* candidate = FindCandidate(choice.candidateId);
    if (candidate == nullptr)
        return std::nullopt;
    return CandidateModelFiles(m_currentRoot, model, candidate->folder);
}

std::vector<HotReload::Request> CItemAbCompare::RequestsFor(const ItemCatalogEntry& item, const Choice& choice,
                                                            std::string& why) const
{
    std::vector<HotReload::Request> requests;
    why = item.name + ": the client loads none of its models";
    for (const ItemModel& model : item.models)
    {
        const std::optional<int> type = Editor::ItemEditor::ModelTypeOf(item, model);
        if (!type || !HotReload::CanReload(*type))
            continue;
        const std::optional<ModelFiles> files = FilesFor(model, choice);
        if (!files)
        {
            why = item.name + ": " + model.bmd + " has no " + ChoiceLabel(choice) + " file";
            continue;
        }
        const std::string modelName =
            item.name + " " + item.key + " (" + Editor::Text::PathToUtf8(files->bmd.filename()) + ")";
        requests.push_back({*type, modelName, choice.variant, files->bmd, files->textureFolders});
    }
    return requests;
}

void CItemAbCompare::StartBatch(const std::string& what, const Choice& choice)
{
    Batch batch;
    batch.what = what;
    batch.choice = choice;
    batch.texturesBefore = Bitmaps.GetNumberOfTexture();
    batch.textureBytesBefore = Bitmaps.GetUsedTextureMemory();
    m_batch = batch;
}

bool CItemAbCompare::QueueItem(const ItemCatalogEntry& item, const Choice& choice)
{
    std::string why;
    const std::vector<HotReload::Request> requests = RequestsFor(item, choice, why);
    if (requests.empty())
    {
        m_status = why + ".";
        return false;
    }
    StartBatch(item.name.empty() ? item.key : item.name, choice);
    for (const HotReload::Request& request : requests)
    {
        if (choice.variant == AssetVariant::Candidate)
            m_queuedCandidate[request.type] = choice.candidateId;
        HotReload::Queue(request);
    }
    m_batch->total = static_cast<int>(requests.size());
    m_status = "Loading " + item.name + " (" + ChoiceLabel(choice) + ")...";
    return true;
}

void CItemAbCompare::QueueMany(const std::vector<const ItemCatalogEntry*>& items, const Choice& choice,
                               const std::string& what)
{
    StartBatch(what, choice);
    std::string why;
    for (const ItemCatalogEntry* item : items)
    {
        for (const HotReload::Request& request : RequestsFor(*item, choice, why))
        {
            HotReload::Queue(request);
            ++m_batch->total;
        }
    }
    m_status = "Loading " + std::to_string(m_batch->total) + " models (" + ChoiceLabel(choice) + ")...";
}

std::optional<CItemAbCompare::Choice> CItemAbCompare::ShownChoice(int type) const
{
    const std::optional<AssetVariant> variant = HotReload::LoadedVariant(type);
    if (!variant)
        return std::nullopt;
    if (*variant != AssetVariant::Candidate)
        return Choice{*variant, ""};
    const auto candidate = m_candidateOfType.find(type);
    return Choice{AssetVariant::Candidate, candidate != m_candidateOfType.end() ? candidate->second : ""};
}

// "as built" until a model is switched; "mixed" when the item's models show
// different versions.
std::string CItemAbCompare::ShownLabel(const ItemCatalogEntry& item) const
{
    std::vector<std::optional<Choice>> shown;
    for (const int type : m_itemTypes)
    {
        if (HotReload::CanReload(type))
            shown.push_back(ShownChoice(type));
    }
    if (shown.empty())
        return "-";
    const bool same = std::all_of(shown.begin(), shown.end(), [&](const auto& choice) { return choice == shown.front(); });
    if (!same)
        return "mixed";
    return shown.front() ? ChoiceLabel(*shown.front()) : VariantName(AssetVariant::AsBuilt);
}

bool CItemAbCompare::IsBusy() const
{
    return HotReload::PendingCount() > 0;
}

void CItemAbCompare::DropCopies()
{
    for (std::unique_ptr<HotReload::ModelCopy>& copy : m_copies)
        HotReload::RetireCopy(std::move(copy));
    m_copies.clear();
    ++m_copiesGeneration;
}

void CItemAbCompare::Release()
{
    m_copies.clear();
    m_copiesKey.clear();
    HotReload::ReleaseAllCopies();
}

void CItemAbCompare::UpdateCopies(const ItemCatalogEntry& item)
{
    const std::string wanted = m_sideBySide ? item.key + std::string(COPIES_KEY_SEPARATOR) +
                                                  VariantName(m_right.variant) + m_right.candidateId
                                            : std::string();
    if (wanted == m_copiesKey)
        return;
    DropCopies();
    m_copiesKey = wanted;
    m_copyMessage.clear();
    if (wanted.empty())
        return;
    std::string why;
    const std::vector<HotReload::Request> requests = RequestsFor(item, m_right, why);
    std::vector<std::string> refusals;
    for (const HotReload::Request& request : requests)
    {
        std::string message;
        std::unique_ptr<HotReload::ModelCopy> copy = HotReload::LoadCopy(request, message);
        Log(message);
        if (copy)
            m_copies.push_back(std::move(copy));
        else
            refusals.push_back(message);
    }
    if (requests.empty())
        refusals.push_back(why);
    m_copyMessage = Editor::Text::Join(refusals, "\n");
}

void CItemAbCompare::PassCompareToPreview(const ItemCatalogEntry& item)
{
    EnsureSetup();
    if (m_repo.empty())
        return;
    EnsureItem(item);
    ApplyPendingDelivery(item);
    UpdateCopies(item);
    if (!m_sideBySide)
        return;
    PreviewCompare compare;
    for (const std::unique_ptr<HotReload::ModelCopy>& copy : m_copies)
        compare.copies.push_back(copy.get());
    compare.leftLabel = LEFT_PREFIX + ShownLabel(item);
    compare.rightLabel = ChoiceLabel(m_right);
    compare.rightMessage = m_copies.empty() ? "Not loaded: " + m_copyMessage : std::string();
    compare.generation = m_copiesGeneration;
    g_ItemPreview.SetCompare(std::move(compare));
}

void CItemAbCompare::Render(const ItemCatalogEntry& item, const ItemCatalog& catalog)
{
    EnsureSetup();
    TakeOutcomes();
    if (m_repo.empty())
        return;
    EnsureItem(item);
    if (!m_originalsChecked)
        CheckOriginals(catalog);
    PollFolderPick(item);
    m_captures.Step();
    if (!ImGui::CollapsingHeader(PANEL_TITLE, ImGuiTreeNodeFlags_DefaultOpen))
        return;
    ImGui::PushID(PANEL_TITLE);
    RenderShown(item);
    RenderClientChoice(item);
    RenderSideBySide();
    RenderSharedTextures(item, catalog);
    RenderManySwitch(item, catalog);
    RenderTools(item);
    RenderOriginalsHint(catalog);
    Editor::StatusLine::Render(m_status);
    ImGui::PopID();
}

void CItemAbCompare::RenderShown(const ItemCatalogEntry& item)
{
    const std::string shown = ShownLabel(item);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("The client shows:");
    ImGui::SameLine();
    const std::optional<Choice> choice = m_itemTypes.empty() ? std::nullopt : ShownChoice(m_itemTypes.front());
    const AssetVariant variant = choice ? choice->variant : AssetVariant::AsBuilt;
    const ImVec4 color = variant == AssetVariant::Original    ? ORIGINAL_COLOR
                         : variant == AssetVariant::Candidate ? CANDIDATE_COLOR
                                                              : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImGui::TextColored(color, "%s", shown.c_str());
    HoverTip("What the running client draws this item with, everywhere (inventory, ground, characters). \"as built\" "
             "until you switch it; \"mixed\" when its models show different versions.");
    ImGui::SameLine();
    ImGui::TextColored(NOTE_COLOR, "(%s loaded)", TextureLine().c_str());
}

void CItemAbCompare::RenderClientChoice(const ItemCatalogEntry& item)
{
    const std::optional<Choice> shown = m_itemTypes.empty() ? std::nullopt : ShownChoice(m_itemTypes.front());
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Show:");
    ImGui::BeginDisabled(IsBusy());
    for (const AssetVariant variant : BASE_VARIANTS)
    {
        const Choice choice{variant, ""};
        const bool enabled = variant != AssetVariant::Original || m_originals != VariantState::Missing;
        const bool isShown = shown ? *shown == choice : variant == AssetVariant::AsBuilt;
        ImGui::SameLine();
        ImGui::BeginDisabled(!enabled);
        if (ImGui::RadioButton(VariantName(variant), isShown))
            QueueItem(item, choice);
        ImGui::EndDisabled();
        HoverTip(VariantTip(variant));
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload from disk"))
    {
        RescanCandidates(item);
        m_copiesKey.clear(); // the right picture reads its files again too
        QueueItem(item, shown.value_or(Choice{AssetVariant::AsBuilt, ""}));
    }
    HoverTip("Reads the files the client shows again (and the right picture's), e.g. after Codex delivered or you "
             "pulled; also looks for new deliveries and pilot variants.");
    RenderCandidateChoice(item, shown);
    ImGui::EndDisabled();
}

void CItemAbCompare::RenderCandidateChoice(const ItemCatalogEntry& item, const std::optional<Choice>& shown)
{
    if (m_candidates.empty())
        return;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Candidates:");
    for (const ItemCandidate& candidate : m_candidates)
    {
        const Choice choice{AssetVariant::Candidate, candidate.id};
        SameLineIfFits(RadioWidth(candidate.label.c_str()));
        if (ImGui::RadioButton(candidate.label.c_str(), shown && *shown == choice))
            QueueItem(item, choice);
        HoverTip(Editor::Text::PathToUtf8(candidate.folder).c_str());
    }
}

void CItemAbCompare::RenderSideBySide()
{
    ImGui::Checkbox("Side by side", &m_sideBySide);
    HoverTip("A second picture next to the preview: the same item from other files, same view, camera, +level and "
             "options. Drag either picture to turn both.");
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_sideBySide);
    RenderRightCombo();
    ImGui::EndDisabled();
    if (m_sideBySide && !m_copyMessage.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
        ImGui::TextWrapped("Right picture: %s", m_copyMessage.c_str());
        ImGui::PopStyleColor();
    }
    if (!m_note.empty())
        ImGui::TextColored(NOTE_COLOR, "%s", m_note.c_str());
}

void CItemAbCompare::RenderRightCombo()
{
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (!ImGui::BeginCombo("##RightPicture", ("right: " + ChoiceLabel(m_right)).c_str()))
        return;
    for (const AssetVariant variant : BASE_VARIANTS)
    {
        const Choice choice{variant, ""};
        const bool enabled = variant != AssetVariant::Original || m_originals != VariantState::Missing;
        if (ImGui::Selectable(VariantName(variant), m_right == choice, enabled ? 0 : ImGuiSelectableFlags_Disabled))
            m_right = choice;
    }
    for (const ItemCandidate& candidate : m_candidates)
    {
        const Choice choice{AssetVariant::Candidate, candidate.id};
        if (ImGui::Selectable(candidate.label.c_str(), m_right == choice))
            m_right = choice;
    }
    ImGui::EndCombo();
}

void CItemAbCompare::RenderSharedTextures(const ItemCatalogEntry& item, const ItemCatalog& catalog)
{
    std::vector<const ItemCandidate*> shownCandidates;
    const std::optional<Choice> shown = m_itemTypes.empty() ? std::nullopt : ShownChoice(m_itemTypes.front());
    if (shown && shown->variant == AssetVariant::Candidate)
        shownCandidates.push_back(FindCandidate(shown->candidateId));
    if (m_sideBySide && m_right.variant == AssetVariant::Candidate && (!shown || !(*shown == m_right)))
        shownCandidates.push_back(FindCandidate(m_right.candidateId));
    for (const ItemCandidate* candidate : shownCandidates)
    {
        if (candidate == nullptr)
            continue;
        const std::vector<std::string> partners = TexturePartners(item, ReplacedTextures(item, candidate->folder));
        if (partners.empty())
            continue;
        ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
        ImGui::TextWrapped("%s replaces textures that %s use too: while the client shows it they show its texture, and "
                           "accepting it changes them.",
                           candidate->label.c_str(), PartnerList(partners, catalog).c_str());
        ImGui::PopStyleColor();
    }
    const std::vector<std::string> partners = TexturePartners(item, SharedTextureFiles(item));
    if (partners.empty())
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, NOTE_COLOR);
    ImGui::TextWrapped("Shares textures with %s: a switch of this item switches them too.",
                       PartnerList(partners, catalog).c_str());
    ImGui::PopStyleColor();
}

void CItemAbCompare::RenderManySwitch(const ItemCatalogEntry& item, const ItemCatalog& catalog)
{
    std::vector<const ItemCatalogEntry*> family;
    std::vector<const ItemCatalogEntry*> all;
    for (const ItemCatalogEntry& entry : catalog.items)
    {
        all.push_back(&entry);
        if (!item.family.empty() && entry.family == item.family)
            family.push_back(&entry);
    }
    ImGui::BeginDisabled(IsBusy());
    const struct
    {
        std::string label;
        const std::vector<const ItemCatalogEntry*>* items;
    } groups[] = {{"Family " + item.family + ":", &family}, {"All items:", &all}};
    for (const auto& group : groups)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(group.label.c_str());
        ImGui::PushID(group.label.c_str());
        for (const AssetVariant variant : BASE_VARIANTS)
        {
            ImGui::SameLine();
            ImGui::BeginDisabled(variant == AssetVariant::Original && m_originals == VariantState::Missing);
            if (ImGui::SmallButton(VariantName(variant)))
                QueueMany(*group.items, Choice{variant, ""}, group.label.substr(0, group.label.size() - 1));
            HoverTip(VariantTip(variant));
            ImGui::EndDisabled();
        }
        ImGui::PopID();
    }
    ImGui::EndDisabled();
    if (m_batch && IsBusy())
    {
        ImGui::SameLine();
        ImGui::Text("loading %d of %d...", m_batch->done, m_batch->total);
    }
}

void CItemAbCompare::RenderTools(const ItemCatalogEntry& item)
{
    const bool picking = Editor::Files::IsOpenFilePending(FilePickRequest::CandidateFile);
    ImGui::BeginDisabled(picking);
    if (ImGui::Button(picking ? "Choose a file..." : "Load candidate from folder..."))
        Editor::Files::RequestOpenFile(FilePickRequest::CandidateFile);
    ImGui::EndDisabled();
    HoverTip("Pick any file in a folder with game files for this item (its .bmd, .OZJ or .OZT, named like the game's): "
             "the folder becomes a candidate for this item until the editor closes.");
    ImGui::SameLine();
    const bool canCapture = m_sideBySide && !m_copies.empty() && !m_captures.IsRunning() && !IsBusy();
    ImGui::BeginDisabled(!canCapture);
    if (ImGui::Button("Capture A/B sheet"))
        m_captures.Start(item.Type(), item.key, item.badges.excellent, ShownLabel(item), ChoiceLabel(m_right), m_repo);
    ImGui::EndDisabled();
    HoverTip("Both pictures from the request capture angles (front, side, back, three-quarter, inventory, worn, glow), "
             "each pair also as one sheet, into out/item-ab/.");
    m_captures.Render();
}

void CItemAbCompare::PollFolderPick(const ItemCatalogEntry& item)
{
    const Editor::Files::FilePickResult pick = Editor::Files::PollOpenFile(FilePickRequest::CandidateFile);
    if (pick.state == Editor::Files::FilePickState::Failed)
        m_status = "The file dialog failed: " + pick.error;
    if (pick.state != Editor::Files::FilePickState::Picked)
        return;
    const fs::path folder = pick.path.parent_path();
    const std::optional<ItemCandidate> candidate = CandidateFromFolder(folder, item);
    if (!candidate)
    {
        m_status = Editor::Text::PathToUtf8(folder) + " holds none of " + item.name +
                   "'s files (its model file or a texture named as in the game).";
        return;
    }
    std::vector<ItemCandidate>& picked = m_pickedFolders[item.key];
    if (std::find(picked.begin(), picked.end(), *candidate) == picked.end())
        picked.push_back(*candidate);
    RescanCandidates(item);
    m_sideBySide = true;
    m_right = {AssetVariant::Candidate, candidate->id};
    m_status = "Added " + candidate->label + " as a candidate for " + item.name + ".";
}

void CItemAbCompare::RenderOriginalsHint(const ItemCatalog& catalog)
{
    if (m_originals == VariantState::Ready)
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
    ImGui::TextWrapped(m_originals == VariantState::Missing
                           ? "The original files are not built yet (out/ab/original). Run this in a terminal:"
                           : "out/ab/original was built from other originals than catalog.json lists. Run this again:");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##MaterializeItems", m_materializeCommand.data(), m_materializeCommand.size() + 1,
                     ImGuiInputTextFlags_ReadOnly);
    if (ImGui::Button("Copy command"))
        ImGui::SetClipboardText(m_materializeCommand.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Check again"))
        CheckOriginals(catalog);
}

#endif // _EDITOR
