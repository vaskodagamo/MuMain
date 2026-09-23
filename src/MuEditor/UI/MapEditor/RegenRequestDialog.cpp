#include "stdafx.h"

#ifdef _EDITOR

#include "RegenRequestDialog.h"

#include "MapEditorFileUtil.h"
#include "MapEditorStatusLine.h"
#include "MapObjectPlace.h"

#include "Assets/CaptureImage.h"
#include "Assets/EditorText.h"
#include "Assets/FileDigest.h"
#include "Assets/GitCheckout.h"
#include "Assets/RequestFolder.h"
#include "Assets/RequestNaming.h"
#include "Camera/CameraManager.h"
#include "Camera/CameraMode.h"
#include "Camera/CameraState.h"
#include "Core/MuEditorCore.h"
#include "Core/OfflineWorld.h"
#include "Core/ViewCapture.h"
#include "Engine/Object/ZzzCharacter.h"   // Hero
#include "Engine/Object/w_ObjectInfo.h"   // class OBJECT
#include "Render/Terrain/ZzzLodTerrain.h" // TERRAIN_SCALE
#include "UI/Console/MuEditorConsoleUI.h"

#include "imgui.h"

#include <algorithm>

using namespace Editor::Assets;
namespace fs = std::filesystem;

namespace
{
constexpr const char* POPUP_TITLE = "Flag for regeneration";
constexpr float DIALOG_WIDTH = 620.0f;
constexpr float MULTILINE_ROWS = 3.5f;
constexpr std::size_t SHORT_SHA_CHARS = 8;

// In RequestKind order.
constexpr const char* KIND_LABELS[] = {"repaint: new textures, same mesh", "remodel: new mesh, same textures",
                                       "repaint + remodel", "new variant: an extra model derived from this one"};
// In RequestPriority order.
constexpr const char* PRIORITY_LABELS[] = {"low", "normal", "high"};

const ImVec4 COLOR_ERROR(1.0f, 0.45f, 0.4f, 1.0f);
constexpr std::string_view LIST_SEPARATOR = ", ";

// The names for the scope preview; "(none)" for an empty list.
std::string ListOrNone(const std::vector<std::string>& items)
{
    return items.empty() ? "(none)" : Editor::Text::Join(items, LIST_SEPARATOR);
}

// `objectFile` is the repository's EncTerrain{N}.obj when it equals the file at
// the checkout's commit, whose record index the request then gives; empty otherwise.
PickedInstance PickedFrom(const OBJECT& object, const fs::path& objectFile)
{
    PickedInstance picked;
    for (int axis = 0; axis < 3; ++axis)
    {
        picked.position[axis] = object.Position[axis];
        picked.rotation[axis] = object.Angle[axis];
    }
    picked.scale = object.Scale;
    picked.tile = {static_cast<double>(object.Position[0]) / TERRAIN_SCALE,
                   static_cast<double>(object.Position[1]) / TERRAIN_SCALE};
    if (objectFile.empty())
        return picked;
    const int record = Editor::ObjectPlace::FindRecordIndex(objectFile, object.Type, object.Position);
    if (record >= 0)
        picked.objIndex = record;
    return picked;
}

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"%hs\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor(message);
}
} // namespace

CRegenRequestDialog& CRegenRequestDialog::GetInstance()
{
    static CRegenRequestDialog instance;
    return instance;
}

void CRegenRequestDialog::Open(int world, const Catalog& catalog, const std::string& model, const OBJECT* picked)
{
    const CatalogModel* primary = catalog.FindByName(model);
    const fs::path& repo = Editor::Files::RepoRoot().root;
    if (primary == nullptr || repo.empty() || m_stage == Stage::Capturing)
        return;

    m_domain = WorldRequestDomain(world, catalog.worldName);
    m_targets.clear();
    RequestTarget first{*primary, Editor::Files::Sha256Hex(repo / primary->bmd), {}};
    const fs::path objectFile = Editor::Files::RepoDataFile(repo, Editor::Files::TerrainObjectFile(world));
    m_objectFileCommitted =
        Editor::Git::IsFileUnchanged(repo, Editor::Text::GenericPathToUtf8(objectFile.lexically_relative(repo)));
    if (picked != nullptr)
        first.picked.push_back(PickedFrom(*picked, m_objectFileCommitted ? objectFile : fs::path()));
    m_targets.push_back(first);
    for (const TextureLink& texture : primary->textures)
    {
        for (const std::string& name : texture.sharedWith)
        {
            const CatalogModel* partner = catalog.FindByName(name);
            const bool known = std::any_of(m_targets.begin(), m_targets.end(),
                                           [&](const RequestTarget& t) { return t.model.name == name; });
            if (partner != nullptr && !known && partner->inScope && partner->type != NO_TYPE)
                m_targets.push_back({*partner, Editor::Files::Sha256Hex(repo / partner->bmd), {}});
        }
    }
    m_takenNames = TakenModelNames(repo, m_domain, catalog);
    ReadCheckout();

    m_kind = static_cast<int>(RequestKind::Repaint);
    m_priority = static_cast<int>(RequestPriority::Normal);
    m_summary[0] = m_details[0] = m_keep[0] = m_avoid[0] = '\0';
    m_includeView = true;
    m_includePartners = false;
    m_pushAllowed = false;
    m_scopeKind = -1;
    m_error.clear();
    m_stage = Stage::Form;
    m_openPending = true;
}

void CRegenRequestDialog::ReadCheckout()
{
    const fs::path& repo = Editor::Files::RepoRoot().root;
    const Editor::Git::HeadInfo head = Editor::Git::ReadHead(repo);
    m_headCommit = head.commit;
    m_headProblem.clear();
    if (head.commit.empty())
        m_headProblem = "Cannot read the checkout's commit: " + head.error;
    else if (!Editor::Git::IsOriginBranchTip(repo, head.commit))
        m_headProblem = "Your checkout is at " + head.commit.substr(0, SHORT_SHA_CHARS) + " (" +
                        (head.branch.empty() ? std::string("detached") : head.branch) +
                        "), which no branch on origin points at. The request records it as base_commit, so push it "
                        "to origin (or file from an up-to-date main) before the art builder starts.";
}

std::optional<CRegenRequestDialog::FiledRequest> CRegenRequestDialog::TakeFiledRequest()
{
    std::optional<FiledRequest> filed = std::move(m_filed);
    m_filed.reset();
    return filed;
}

RequestKind CRegenRequestDialog::Kind() const
{
    return static_cast<RequestKind>(m_kind);
}

bool CRegenRequestDialog::IncludesPartners() const
{
    return m_includePartners && Kind() != RequestKind::NewVariant;
}

std::vector<RequestTarget> CRegenRequestDialog::Targets() const
{
    if (m_targets.empty() || IncludesPartners())
        return m_targets;
    return {m_targets.front()};
}

const RequestScope& CRegenRequestDialog::Scope()
{
    if (m_scopeKind != m_kind || m_scopeWithPartners != IncludesPartners())
    {
        m_scope = ComputeScope(Kind(), Targets());
        m_scopeKind = m_kind;
        m_scopeWithPartners = IncludesPartners();
    }
    return m_scope;
}

void CRegenRequestDialog::Render()
{
    if (m_stage == Stage::Closed)
        return;
    if (m_stage == Stage::Capturing)
        FinishCapture();
    if (m_openPending)
    {
        ImGui::OpenPopup(POPUP_TITLE);
        m_openPending = false;
    }

    // Fixed width; the height follows the content (the form and the result differ).
    ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, 0.0f), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal(POPUP_TITLE, nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        if (m_stage != Stage::Capturing)
            m_stage = Stage::Closed;
        return;
    }
    g_MuEditorCore.SetHoveringUI(true); // clicks in the dialog never reach the world
    if (m_stage == Stage::Done)
        RenderDone();
    else if (m_stage == Stage::Capturing)
        ImGui::TextUnformatted("Capturing the view...");
    else
        RenderForm();
    ImGui::EndPopup();
}

void CRegenRequestDialog::RenderForm()
{
    const CatalogModel& model = m_targets.front().model;
    ImGui::Text("%s (type %d)", model.name.c_str(), model.type);
    ImGui::TextWrapped("%s", model.identity.c_str());
    ImGui::Separator();
    RenderOwnerFields();
    RenderScopePreview();
    RenderWarnings();
    if (!m_error.empty())
        ImGui::TextColored(COLOR_ERROR, "%s", m_error.c_str());

    ImGui::Separator();
    ImGui::BeginDisabled(Editor::Text::Trim(m_summary).empty());
    if (ImGui::Button("Create"))
        Create();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        m_stage = Stage::Closed;
        ImGui::CloseCurrentPopup();
    }
}

void CRegenRequestDialog::RenderOwnerFields()
{
    ImGui::Combo("Kind", &m_kind, KIND_LABELS, IM_ARRAYSIZE(KIND_LABELS));
    ImGui::Combo("Priority", &m_priority, PRIORITY_LABELS, IM_ARRAYSIZE(PRIORITY_LABELS));
    ImGui::InputTextWithHint("Summary", "One sentence: what should change?", m_summary, sizeof(m_summary));
    const ImVec2 box(-FLT_MIN, ImGui::GetTextLineHeight() * MULTILINE_ROWS);
    ImGui::TextUnformatted("Details: what exactly to change (one item per line)");
    ImGui::InputTextMultiline("##Details", m_details, sizeof(m_details), box);
    ImGui::TextUnformatted("Keep: what must stay as it is");
    ImGui::InputTextMultiline("##Keep", m_keep, sizeof(m_keep), box);
    ImGui::TextUnformatted("Avoid: what the new version must not do");
    ImGui::InputTextMultiline("##Avoid", m_avoid, sizeof(m_avoid), box);

    if (m_targets.size() > 1 && Kind() != RequestKind::NewVariant)
    {
        std::vector<std::string> partners;
        for (std::size_t i = 1; i < m_targets.size(); ++i)
            partners.push_back(m_targets[i].model.name);
        const std::string label = "Also rework " + ListOrNone(partners) + " (they share its textures)";
        ImGui::Checkbox(label.c_str(), &m_includePartners);
    }
    ImGui::Checkbox("Include the current view (a screenshot without the editor)", &m_includeView);
    ImGui::Checkbox("Codex may push its branch and open a PR (never merge)", &m_pushAllowed);
}

void CRegenRequestDialog::RenderScopePreview()
{
    const RequestScope& scope = Scope();
    ImGui::TextDisabled("The art builder may replace: %s", ListOrNone(scope.ownedFiles).c_str());
    if (!scope.frozenTextures.empty())
        ImGui::TextDisabled("Frozen (shared with other models): %s", ListOrNone(scope.frozenTextures).c_str());
    if (Kind() == RequestKind::NewVariant)
    {
        const std::string name = NextVariantName(m_targets.front().model.name, m_takenNames);
        ImGui::TextDisabled("New model: %s (installs nothing; you add it to the map's model table yourself)",
                            name.empty() ? "no free name" : name.c_str());
    }
    const PickedInstance* picked = m_targets.front().picked.empty() ? nullptr : &m_targets.front().picked.front();
    if (picked == nullptr)
    {
        ImGui::TextDisabled("No instance picked (select one with Prev/Next or in the Objects tab to point at it).");
        return;
    }
    ImGui::TextDisabled("Picked instance at tile (%.1f, %.1f)", picked->tile[0], picked->tile[1]);
    if (m_objectFileCommitted)
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Its record number in the map's objects file is left out: your checkout's copy has "
                       "uncommitted changes, so it is numbered differently than at your commit.");
    ImGui::PopStyleColor();
}

void CRegenRequestDialog::RenderWarnings()
{
    ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
    if (!m_headProblem.empty())
        ImGui::TextWrapped("%s", m_headProblem.c_str());
    // Every model the request names, partners included: each one's current_sha256
    // must be its BMD at base_commit.
    for (const RequestTarget& target : Targets())
    {
        if (target.sha256 != target.model.currentSha256)
            ImGui::TextWrapped("%s in your checkout differs from catalog.json (an uncommitted change, or the catalog "
                               "is out of date). A request describes the committed file: commit or restore it, or "
                               "re-run build_editor_catalog.py, then open this dialog again.",
                               target.model.bmd.c_str());
    }
    const bool paints = Kind() == RequestKind::Repaint || Kind() == RequestKind::RepaintRemodel;
    if (paints && Scope().ownedFiles.empty())
        ImGui::TextWrapped("Every texture of %s is shared with another model, so a repaint could change nothing. "
                           "Tick 'Also rework ...' or choose remodel.",
                           m_targets.front().model.name.c_str());
    ImGui::PopStyleColor();
}

void CRegenRequestDialog::RenderDone()
{
    const std::string folder = RequestFolderPath(m_domain, m_draft.id);
    ImGui::Text("Request written:");
    ImGui::TextWrapped("%s", Editor::Files::PathToUtf8(m_folder).c_str());
    if (ImGui::Button("Open folder"))
    {
        std::string error;
        if (!Editor::Files::OpenWithSystem(m_folder, error))
            m_error = error;
    }
    ImGui::SameLine();
    if (ImGui::Button("Close"))
    {
        m_stage = Stage::Closed;
        ImGui::CloseCurrentPopup();
    }
    ImGui::Separator();
    ImGui::TextWrapped("Codex only sees it once it is on main. Commit just this folder and push it:");
    ImGui::TextDisabled("git add %s", folder.c_str());
    ImGui::TextDisabled("git commit -m \"docs(assets): file request %s\"", m_draft.id.c_str());
    ImGui::TextDisabled("git push   (on main; from another branch, cherry-pick the commit onto main)");
    ImGui::TextWrapped("The coordinator then assigns it to a worker; see %s/README.md.",
                       RequestsFolderPath(m_domain).c_str());
    if (!m_error.empty())
        ImGui::TextColored(COLOR_ERROR, "%s", m_error.c_str());
}

void CRegenRequestDialog::Create()
{
    m_error.clear();
    if (!BuildDraft(m_error))
        return;
    if (!m_includeView)
    {
        Write({});
        return;
    }
    if (!Editor::ViewCapture::Request())
    {
        m_error = "Another capture is still running; try again.";
        return;
    }
    m_stage = Stage::Capturing;
}

bool CRegenRequestDialog::BuildDraft(std::string& error)
{
    const fs::path& repo = Editor::Files::RepoRoot().root;
    if (m_headCommit.empty())
    {
        error = m_headProblem;
        return false;
    }
    const std::vector<RequestTarget> targets = Targets();
    for (const RequestTarget& target : targets)
    {
        if (target.sha256.empty())
        {
            error = "Cannot read " + target.model.bmd + " in the checkout.";
            return false;
        }
    }

    RequestDraft draft;
    const Timestamp now = CurrentTimestamp();
    draft.created = now.dateTime;
    draft.domain = m_domain;
    draft.baseCommit = m_headCommit;
    draft.input.kind = Kind();
    draft.input.priority = static_cast<RequestPriority>(m_priority);
    draft.input.summary = Editor::Text::Trim(m_summary);
    draft.input.details = Editor::Text::NonEmptyLines(m_details);
    draft.input.keep = Editor::Text::NonEmptyLines(m_keep);
    draft.input.avoid = Editor::Text::NonEmptyLines(m_avoid);
    draft.input.pushAllowed = m_pushAllowed;
    draft.targets = targets;
    if (Kind() == RequestKind::NewVariant)
    {
        draft.newModel = NextVariantName(targets.front().model.name, m_takenNames);
        if (draft.newModel->empty())
        {
            error = "No free name for a new variant of " + targets.front().model.name + ".";
            return false;
        }
    }
    draft.id = MakeRequestId(now.date, targets.front().model.name, MakeSlug(draft.input.summary),
                             ExistingRequestIds(repo, m_domain));
    if (m_includeView)
        draft.captures.push_back(CurrentView());
    m_draft = std::move(draft);
    return true;
}

// The camera and hero of this frame, the one the capture takes.
CaptureInfo CRegenRequestDialog::CurrentView() const
{
    CaptureInfo view;
    for (int axis = 0; axis < 3; ++axis)
    {
        view.cameraPosition[axis] = g_Camera.Position[axis];
        view.cameraAngle[axis] = g_Camera.Angle[axis];
    }
    view.freeFly = CameraManager::Instance().GetCurrentMode() == CameraMode::FreeFly;
    if (!view.freeFly)
        view.cameraDistance = g_Camera.Distance;
    if (!Editor::OfflineWorld::IsActive() && Hero != nullptr)
        view.heroTile =
            std::array<float, 2>{Hero->Object.Position[0] / TERRAIN_SCALE, Hero->Object.Position[1] / TERRAIN_SCALE};
    view.clientCommit = m_headCommit.substr(0, SHORT_SHA_CHARS);
    if (!m_targets.front().picked.empty())
        view.note = "The picked instance has the editor's yellow selection outline (leaves can hide it).";
    return view;
}

void CRegenRequestDialog::FinishCapture()
{
    mu::FramePixels pixels;
    const Editor::ViewCapture::Result result = Editor::ViewCapture::Collect(pixels);
    if (result == Editor::ViewCapture::Result::Waiting)
        return;
    m_stage = Stage::Form;
    if (result == Editor::ViewCapture::Result::Failed)
    {
        m_error = "The view could not be captured. Try again, or untick 'Include the current view'.";
        return;
    }
    const mu::FramePixels small = Editor::Capture::DownscaleToWidth(pixels, Editor::Capture::MAX_CAPTURE_WIDTH);
    const std::vector<std::uint8_t> jpeg = Editor::Capture::EncodeJpeg(small, Editor::Capture::CAPTURE_JPEG_QUALITY);
    if (jpeg.empty())
    {
        m_error = "The capture could not be encoded as a JPEG.";
        return;
    }
    m_draft.captures.front().width = static_cast<int>(small.width);
    m_draft.captures.front().height = static_cast<int>(small.height);
    Write(jpeg);
}

void CRegenRequestDialog::Write(const std::vector<std::uint8_t>& jpeg)
{
    std::string error;
    if (!WriteRequestFolder(Editor::Files::RepoRoot().root, m_draft, jpeg, m_folder, error))
    {
        m_error = "Nothing was written: " + error;
        m_stage = Stage::Form;
        return;
    }
    m_filed = FiledRequest{m_draft.targets.front().model.name, m_draft.id};
    m_stage = Stage::Done;
    Log("[Assets] Filed regeneration request " + Editor::Files::PathToUtf8(m_folder));
}

#endif // _EDITOR
