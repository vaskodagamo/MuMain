#include "stdafx.h"

#ifdef _EDITOR

#include "MapAssetReview.h"

#include "MapAssetVariants.h"
#include "MapEditorFileUtil.h"
#include "MapEditorStatusLine.h"
#include "MapObjectPlace.h"
#include "RegenRequestDialog.h"

#include "Assets/EditorText.h"
#include "Assets/RequestNaming.h"
#include "Core/EditorCamera.h"
#include "Engine/Object/w_ObjectInfo.h" // class OBJECT

#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <unordered_set>

using namespace Editor::Assets;

// Outlines every object whose type is in the set (ZzzObject.cpp).
extern std::unordered_set<int> g_MapEditorHighlightedTypes;

namespace
{
// Status filter entries: "All", the catalog's statuses, then two review filters.
constexpr const char* STATUS_FILTERS[] = {"All",     "accepted",   "in-progress", "unchanged",
                                          "blocked", "needs work", "open request"};
constexpr int FILTER_ALL = 0;
constexpr int FILTER_FIRST_STATUS = 1;
constexpr int FILTER_LAST_STATUS = 4;
constexpr int FILTER_NEEDS_WORK = 5;
constexpr int FILTER_OPEN_REQUEST = 6;

enum ColumnId
{
    COLUMN_NAME,
    COLUMN_TYPE,
    COLUMN_STATUS,
    COLUMN_PLACED,
    COLUMN_CLIENT,
    COLUMN_SHOWS,
    COLUMN_IDENTITY,
    COLUMN_COUNT,
};

// The table takes this share of the tab's height; the details get the rest.
constexpr float TABLE_HEIGHT_SHARE = 0.42f;
constexpr float FILTER_WIDTH_SHARE = 0.55f;
constexpr float MIN_TABLE_HEIGHT = 140.0f;
constexpr float NAME_COLUMN_WIDTH = 96.0f;
constexpr float TYPE_COLUMN_WIDTH = 36.0f;
constexpr float STATUS_COLUMN_WIDTH = 78.0f;
constexpr float PLACED_COLUMN_WIDTH = 48.0f;
constexpr float CLIENT_COLUMN_WIDTH = 70.0f;
constexpr float SHOWS_COLUMN_WIDTH = 56.0f;
constexpr float NOTE_FIELD_WIDTH = 220.0f;

const ImVec4 COLOR_GOOD(0.5f, 1.0f, 0.5f, 1.0f);
const ImVec4 COLOR_BAD(1.0f, 0.5f, 0.4f, 1.0f);

ImVec4 StatusColor(const std::string& status)
{
    if (status == "accepted")
        return COLOR_GOOD;
    if (status == "blocked")
        return COLOR_BAD;
    if (status == "in-progress")
        return Editor::StatusLine::WarningColor();
    return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
}

const char* VerdictLabel(const ClientReview* review)
{
    if (review == nullptr || review->verdict.empty())
        return "-";
    return review->verdict == VERDICT_LOOKS_GOOD ? "looks good" : "needs work";
}

// Only a placed World object type inside the art scope can be requested.
bool CanFlag(const CatalogModel& model)
{
    return model.type != NO_TYPE && model.inScope;
}

} // namespace

CMapAssetReview& CMapAssetReview::GetInstance()
{
    static CMapAssetReview instance;
    return instance;
}

void CMapAssetReview::EnsureCatalog(int world)
{
    if (world == m_world)
        return;
    m_world = world;
    m_load = {};
    m_reviews.clear();
    m_reviewError.clear();
    m_instanceIndex = -1;
    g_MapAssetVariants.Reset();
    const std::filesystem::path& repo = Editor::Files::RepoRoot().root;
    if (repo.empty())
        return;
    m_load = LoadCatalog(repo, world);
    m_reviews = ReadClientReviews(ClientReviewFile(repo, world), m_reviewError);
}

const std::string* CMapAssetReview::CatalogNameOf(int world, int type)
{
    EnsureCatalog(world);
    const CatalogModel* model = m_load.catalog ? m_load.catalog->FindByType(type) : nullptr;
    return model != nullptr ? &model->name : nullptr;
}

const CatalogModel* CMapAssetReview::SelectedModel() const
{
    return m_load.catalog ? m_load.catalog->FindByName(m_selectedName) : nullptr;
}

const ClientReview* CMapAssetReview::VerdictOf(const CatalogModel& model) const
{
    const auto it = m_reviews.find(model.name);
    if (it != m_reviews.end())
        return &it->second;
    return model.clientReview ? &*model.clientReview : nullptr;
}

void CMapAssetReview::Render(int world, OBJECT*& selected)
{
    EnsureCatalog(world);
    if (RenderCatalogProblem(world))
        return;

    g_MapAssetVariants.RenderAllModelsSwitch(world, *m_load.catalog);
    NoteFiledRequests();
    Editor::ObjectPlace::CountLiveObjects(m_liveCounts);
    RenderFilters();
    RebuildRows();
    RenderTable();
    ImGui::Separator();
    RenderDetails(selected);
    UpdateHighlight();
}

bool CMapAssetReview::RenderCatalogProblem(int world)
{
    const Editor::Files::RepoRootLookup& repo = Editor::Files::RepoRoot();
    if (repo.root.empty())
    {
        ImGui::TextColored(Editor::StatusLine::WarningColor(), "No repository found (%s).", repo.description.c_str());
        ImGui::TextWrapped("The Assets tab reads assets-work/ from your checkout. Start the client from inside it, or "
                           "set MU_EDITOR_REPO_ROOT.");
        return true;
    }
    if (m_load.catalog)
    {
        ImGui::Text("%s: %d models", m_load.catalog->worldName.c_str(), (int)m_load.catalog->models.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload"))
            m_world = -1; // read catalog.json and client-review.json again next frame
        return false;
    }
    if (!m_load.fileFound)
        ImGui::Text("No catalog for this world (World%d).", world);
    else
        ImGui::TextColored(COLOR_BAD, "catalog.json could not be read: %s", m_load.error.c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", Editor::Files::PathToUtf8(CatalogFile(repo.root, world)).c_str());
    ImGui::PopStyleColor();
    if (ImGui::Button("Reload"))
        m_world = -1;
    return true;
}

void CMapAssetReview::RenderFilters()
{
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * FILTER_WIDTH_SHARE);
    ImGui::InputTextWithHint("##AssetFilter", "Filter by name or identity", m_filter, sizeof(m_filter));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##AssetStatus", &m_statusFilter, STATUS_FILTERS, IM_ARRAYSIZE(STATUS_FILTERS));
}

void CMapAssetReview::RebuildRows()
{
    const std::string_view needle = m_filter;
    const std::vector<CatalogModel>& models = m_load.catalog->models;
    m_rows.clear();
    for (int i = 0; i < (int)models.size(); ++i)
    {
        const CatalogModel& model = models[i];
        if (!Editor::Text::ContainsIgnoringCase(model.name, needle) &&
            !Editor::Text::ContainsIgnoringCase(model.identity, needle))
            continue;
        const ClientReview* review = VerdictOf(model);
        const bool statusMatches =
            m_statusFilter == FILTER_ALL ||
            (m_statusFilter >= FILTER_FIRST_STATUS && m_statusFilter <= FILTER_LAST_STATUS &&
             model.status == STATUS_FILTERS[m_statusFilter]) ||
            (m_statusFilter == FILTER_NEEDS_WORK && review != nullptr && review->verdict == VERDICT_NEEDS_WORK) ||
            (m_statusFilter == FILTER_OPEN_REQUEST && !model.requests.empty());
        if (statusMatches)
            m_rows.push_back(i);
    }
}

void CMapAssetReview::RenderTable()
{
    const float height = std::max(MIN_TABLE_HEIGHT, ImGui::GetContentRegionAvail().y * TABLE_HEIGHT_SHARE);
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
    if (!ImGui::BeginTable("AssetTable", COLUMN_COUNT, flags, ImVec2(0.0f, height)))
        return;
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort,
                            NAME_COLUMN_WIDTH, COLUMN_NAME);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, TYPE_COLUMN_WIDTH, COLUMN_TYPE);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, STATUS_COLUMN_WIDTH, COLUMN_STATUS);
    ImGui::TableSetupColumn("Placed", ImGuiTableColumnFlags_WidthFixed, PLACED_COLUMN_WIDTH, COLUMN_PLACED);
    ImGui::TableSetupColumn("Client", ImGuiTableColumnFlags_WidthFixed, CLIENT_COLUMN_WIDTH, COLUMN_CLIENT);
    ImGui::TableSetupColumn("Shows", ImGuiTableColumnFlags_WidthFixed, SHOWS_COLUMN_WIDTH, COLUMN_SHOWS);
    ImGui::TableSetupColumn("Identity", ImGuiTableColumnFlags_WidthStretch, 0.0f, COLUMN_IDENTITY);
    ImGui::TableHeadersRow();

    if (const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs != nullptr && specs->SpecsCount > 0)
        SortRows(specs->Specs[0].ColumnUserID, specs->Specs[0].SortDirection != ImGuiSortDirection_Descending);

    for (int index : m_rows)
        RenderTableRow(index);
    m_scrollToSelected = false;
    ImGui::EndTable();
}

int CMapAssetReview::LiveCount(int type) const
{
    return type >= 0 && type < (int)m_liveCounts.size() ? m_liveCounts[type] : 0;
}

void CMapAssetReview::SortRows(ImGuiID column, bool ascending)
{
    const std::vector<CatalogModel>& models = m_load.catalog->models;
    // Negative: a before b; ties fall back to the name, so the order never flickers.
    const auto compare = [&](const CatalogModel& a, const CatalogModel& b) -> int
    {
        switch (column)
        {
        case COLUMN_TYPE:
            return a.type - b.type;
        case COLUMN_STATUS:
            return a.status.compare(b.status);
        case COLUMN_PLACED:
            return LiveCount(a.type) - LiveCount(b.type);
        case COLUMN_CLIENT:
            return std::strcmp(VerdictLabel(VerdictOf(a)), VerdictLabel(VerdictOf(b)));
        case COLUMN_SHOWS:
            return std::strcmp(g_MapAssetVariants.ShownLabel(a), g_MapAssetVariants.ShownLabel(b));
        case COLUMN_IDENTITY:
            return a.identity.compare(b.identity);
        default:
            return 0;
        }
    };
    std::sort(m_rows.begin(), m_rows.end(),
              [&](int left, int right)
              {
                  const CatalogModel& a = models[ascending ? left : right];
                  const CatalogModel& b = models[ascending ? right : left];
                  const int order = compare(a, b);
                  return order != 0 ? order < 0 : a.name < b.name;
              });
}

void CMapAssetReview::RenderTableRow(int index)
{
    const CatalogModel& model = m_load.catalog->models[index];
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(COLUMN_NAME);
    const bool isSelected = model.name == m_selectedName;
    if (ImGui::Selectable(model.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns) && !isSelected)
    {
        m_selectedName = model.name;
        m_instanceIndex = -1;
    }
    if (isSelected && m_scrollToSelected)
        ImGui::SetScrollHereY();

    ImGui::TableSetColumnIndex(COLUMN_TYPE);
    if (model.type == NO_TYPE)
        ImGui::TextDisabled("-");
    else
        ImGui::Text("%d", model.type);
    ImGui::TableSetColumnIndex(COLUMN_STATUS);
    ImGui::TextColored(StatusColor(model.status), "%s", model.status.c_str());
    ImGui::TableSetColumnIndex(COLUMN_PLACED);
    const int live = LiveCount(model.type);
    ImGui::Text("%d", live);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%d on the loaded map now, %d in the shipped map file", live, model.placementCount);
    ImGui::TableSetColumnIndex(COLUMN_CLIENT);
    ImGui::TextUnformatted(VerdictLabel(VerdictOf(model)));
    ImGui::TableSetColumnIndex(COLUMN_SHOWS);
    ImGui::TextUnformatted(g_MapAssetVariants.ShownLabel(model));
    ImGui::TableSetColumnIndex(COLUMN_IDENTITY);
    ImGui::TextUnformatted(model.identity.c_str());
}

void CMapAssetReview::RenderDetails(OBJECT*& selected)
{
    ImGui::BeginChild("AssetDetails", ImVec2(0.0f, 0.0f), false);
    const CatalogModel* model = SelectedModel();
    if (model == nullptr)
    {
        ImGui::TextDisabled("Select a model in the table.");
        ImGui::EndChild();
        return;
    }

    ImGui::Text("%s", model->name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(StatusColor(model->status), "%s", model->status.c_str());
    ImGui::TextWrapped("%s", model->identity.c_str());
    if (!model->inScope)
        ImGui::TextColored(COLOR_BAD, "Outside the art scope: %s", model->exclusion.c_str());
    ImGui::TextDisabled("%s%s", model->bmd.c_str(), model->bmdModified ? " (rebuilt)" : " (original)");
    g_MapAssetVariants.RenderModelSwitch(*model);

    RenderInstanceActions(*model, selected);
    RenderVerdict(*model);
    RenderTextures(*model);
    RenderHistory(*model);
    Editor::StatusLine::Render(m_status);
    ImGui::EndChild();
}

void CMapAssetReview::RenderInstanceActions(const CatalogModel& model, OBJECT*& selected)
{
    ImGui::Separator();
    const int live = LiveCount(model.type);
    ImGui::BeginDisabled(model.type == NO_TYPE);
    ImGui::Checkbox("Highlight all", &m_highlight);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##PrevInstance", ImGuiDir_Left))
        StepInstance(model, -1, selected);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##NextInstance", ImGuiDir_Right))
        StepInstance(model, 1, selected);
    ImGui::SameLine();
    if (m_instanceIndex >= 0 && live > 0)
        ImGui::Text("instance %d of %d", m_instanceIndex + 1, live);
    else
        ImGui::Text("%d placed", live);
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!CanFlag(model));
    if (ImGui::Button("Flag for regeneration..."))
        OpenFlagDialog(model, selected);
    ImGui::EndDisabled();
    if (!CanFlag(model) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Only placed models inside the art scope can be requested.");
    for (const RequestRef& request : model.requests)
    {
        ImGui::BulletText("Request %s: %s%s%s", request.id.c_str(), request.status.c_str(),
                          request.assignedTo.empty() ? "" : ", assigned to ", request.assignedTo.c_str());
    }
}

void CMapAssetReview::RenderVerdict(const CatalogModel& model)
{
    const ClientReview* review = VerdictOf(model);
    ImGui::Text("In the client: %s", VerdictLabel(review));
    if (review != nullptr && !review->note.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s, %s)", review->note.c_str(), review->date.c_str());
    }
    ImGui::SetNextItemWidth(NOTE_FIELD_WIDTH);
    ImGui::InputTextWithHint("##VerdictNote", "Note (optional)", m_verdictNote, sizeof(m_verdictNote));
    ImGui::SameLine();
    if (ImGui::Button("Looks good in client"))
        RecordVerdict(model, VERDICT_LOOKS_GOOD);
    ImGui::SameLine();
    if (ImGui::Button("Needs work"))
        RecordVerdict(model, VERDICT_NEEDS_WORK);
}

void CMapAssetReview::RenderTextures(const CatalogModel& model)
{
    if (!ImGui::CollapsingHeader("Textures and engine rules", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    for (const TextureLink& texture : model.textures)
    {
        ImGui::BulletText("%s -> %s", texture.name.c_str(), texture.container.c_str());
        if (texture.sharedWith.empty())
            continue;
        std::string users;
        for (const std::string& other : texture.sharedWith)
            users += (users.empty() ? "" : ", ") + other;
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
        ImGui::TextWrapped("shared with %s", users.c_str());
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }
    for (const EngineControl& control : model.engineControls)
    {
        std::string controls;
        for (const std::string& item : control.controls)
            controls += (controls.empty() ? "" : "; ") + item;
        ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
        ImGui::TextWrapped("Engine: %s. %s", controls.c_str(), control.requirement.c_str());
        ImGui::PopStyleColor();
    }
}

void CMapAssetReview::RenderHistory(const CatalogModel& model)
{
    if (!ImGui::CollapsingHeader("Batches and previews"))
        return;
    for (auto batch = model.batches.rbegin(); batch != model.batches.rend(); ++batch)
    {
        ImGui::BulletText("%s (%s%s%s)", batch->name.c_str(), batch->role.c_str(), batch->agent.empty() ? "" : ", ",
                          batch->agent.c_str());
        ImGui::Indent();
        ImGui::TextDisabled("%s", batch->notes.c_str());
        ImGui::Unindent();
    }
    if (model.batches.empty())
        ImGui::TextDisabled("No batch has changed this model.");
    ImGui::TextDisabled("Original: %s", model.original.revision.c_str());

    ImGui::BeginDisabled(model.baselinePreview.empty());
    if (ImGui::Button("Open baseline preview"))
        OpenRepoFile(model.baselinePreview);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(model.finalPreview.empty());
    if (ImGui::Button("Open final preview"))
        OpenRepoFile(model.finalPreview);
    ImGui::EndDisabled();
}

void CMapAssetReview::StepInstance(const CatalogModel& model, int step, OBJECT*& selected)
{
    const std::vector<OBJECT*> instances = Editor::ObjectPlace::LiveObjectsOfType(model.type);
    if (instances.empty())
    {
        m_status = model.name + " is not placed on this map.";
        return;
    }
    const int count = (int)instances.size();
    const auto current = std::find(instances.begin(), instances.end(), selected);
    int index = current != instances.end() ? (int)(current - instances.begin()) : m_instanceIndex;
    if (index < 0)
        index = step > 0 ? count - 1 : 0; // the first step lands on the first or last instance
    m_instanceIndex = ((index + step) % count + count) % count;
    selected = instances[m_instanceIndex];
    if (!Editor::Camera::FocusOn(selected))
        m_status = "Could not switch to the free-fly camera.";
    else
        m_status.clear();
}

void CMapAssetReview::RecordVerdict(const CatalogModel& model, const char* verdict)
{
    const std::filesystem::path file = ClientReviewFile(Editor::Files::RepoRoot().root, m_world);
    std::string error;
    if (!RecordClientReview(file, model.name, {verdict, m_verdictNote, CurrentTimestamp().date}, error))
    {
        m_status = "Verdict NOT saved: " + error;
        return;
    }
    m_reviews = ReadClientReviews(file, m_reviewError);
    m_verdictNote[0] = '\0';
    m_status = "Saved in " + Editor::Files::PathToUtf8(file) +
               ". Commit it; build_editor_catalog.py copies it into catalog.json.";
}

void CMapAssetReview::OpenFlagDialog(const CatalogModel& model, const OBJECT* picked)
{
    const OBJECT* instance = (picked != nullptr && picked->Type == model.type) ? picked : nullptr;
    g_RegenRequestDialog.Open(m_world, *m_load.catalog, model.name, instance);
}

void CMapAssetReview::NoteFiledRequests()
{
    const std::optional<CRegenRequestDialog::FiledRequest> filed = g_RegenRequestDialog.TakeFiledRequest();
    if (!filed || !m_load.catalog)
        return;
    for (CatalogModel& model : m_load.catalog->models)
    {
        if (model.name == filed->model)
            model.requests.push_back({filed->id, "open", ""});
    }
}

void CMapAssetReview::OpenRepoFile(const std::string& repoPath)
{
    std::string error;
    if (!Editor::Files::OpenWithSystem(Editor::Files::RepoRoot().root / Editor::Text::Utf8Path(repoPath), error))
        m_status = error;
}

void CMapAssetReview::UpdateHighlight()
{
    const CatalogModel* model = SelectedModel();
    const int wanted = (m_highlight && model != nullptr) ? model->type : NO_TYPE;
    const bool unchanged =
        wanted == NO_TYPE ? g_MapEditorHighlightedTypes.empty()
                          : (g_MapEditorHighlightedTypes.size() == 1 && g_MapEditorHighlightedTypes.contains(wanted));
    if (unchanged)
        return;
    g_MapEditorHighlightedTypes.clear();
    if (wanted != NO_TYPE)
        g_MapEditorHighlightedTypes.insert(wanted);
}

void CMapAssetReview::ClearHighlight()
{
    g_MapEditorHighlightedTypes.clear();
}

bool CMapAssetReview::RenderObjectSummary(int world, const OBJECT* selected)
{
    EnsureCatalog(world);
    if (selected == nullptr || !m_load.catalog)
        return false;
    NoteFiledRequests();
    const CatalogModel* model = m_load.catalog->FindByType(selected->Type);
    if (model == nullptr)
    {
        ImGui::TextDisabled("Type %d is not in the asset catalog.", selected->Type);
        return false;
    }

    ImGui::Text("%s", model->name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(StatusColor(model->status), "%s", model->status.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("client: %s", VerdictLabel(VerdictOf(*model)));
    ImGui::TextWrapped("%s", model->identity.c_str());
    ImGui::BeginDisabled(!CanFlag(*model));
    if (ImGui::Button("Flag for regeneration..."))
        OpenFlagDialog(*model, selected);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (!ImGui::Button("Show in Assets tab"))
        return false;
    m_selectedName = model->name;
    m_scrollToSelected = true;
    return true;
}

#endif // _EDITOR
