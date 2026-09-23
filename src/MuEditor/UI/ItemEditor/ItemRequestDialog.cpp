#include "stdafx.h"

#ifdef _EDITOR

#include "ItemRequestDialog.h"

#include "ItemRequestWatch.h"

#include "Assets/CaptureImage.h"
#include "Assets/EditorText.h"
#include "Assets/FileDigest.h"
#include "Assets/GitCheckout.h"
#include "Assets/RequestFolder.h"
#include "Assets/RequestNaming.h"
#include "Core/EditorFiles.h"
#include "Core/MuEditorCore.h"
#include "Render/Renderer/MuRenderer.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "UI/MapEditor/MapEditorFilePicker.h"
#include "UI/MapEditor/MapEditorStatusLine.h"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <system_error>

using namespace Editor::Assets;
namespace fs = std::filesystem;

namespace
{
constexpr const char* POPUP_TITLE = "Ask Codex";
constexpr float DIALOG_WIDTH = 680.0f;
constexpr float MULTILINE_ROWS = 3.0f;
constexpr float CONCEPT_THUMB_SIZE = 128.0f;
constexpr float REPORT_ROWS = 8.0f;
constexpr std::uint32_t CONCEPT_THUMB_WIDTH = 256; // pixels uploaded for the thumbnail
constexpr std::size_t SHORT_SHA_CHARS = 8;
constexpr const char* CONCEPT_REFERENCE = "ref-concept.jpg";
constexpr std::size_t REFERENCE_NAME_CHARS = 32;
constexpr const char* VALIDATOR_SCRIPT = "validate_request.py";

// In ItemRequestKind order.
constexpr const char* KIND_LABELS[ITEM_REQUEST_KIND_COUNT] = {
    "upscale: same design, better textures",
    "repaint: new colours or materials, same mesh",
    "remodel: better geometry, same silhouette and function",
    "redesign: a new look for the same item",
    "set: every part of the armour set together",
};
// The kinds a set applies to its parts (all but Set).
constexpr int PART_KIND_COUNT = ITEM_REQUEST_KIND_COUNT - 1;
// In RequestPriority order.
constexpr const char* PRIORITY_LABELS[] = {"low", "normal", "high"};

const ImVec4 COLOR_ERROR(1.0f, 0.45f, 0.4f, 1.0f);
const ImVec4 COLOR_OK(0.45f, 0.85f, 0.45f, 1.0f);
constexpr std::string_view LIST_SEPARATOR = ", ";

std::string ListOrNone(const std::vector<std::string>& items)
{
    return items.empty() ? "(none)" : Editor::Text::Join(items, LIST_SEPARATOR);
}

void CopyText(char* buffer, std::size_t size, const std::string& text)
{
    const std::size_t count = std::min(text.size(), size - 1);
    std::copy_n(text.data(), count, buffer);
    buffer[count] = '\0';
}

std::string Lines(const std::vector<std::string>& lines)
{
    return Editor::Text::Join(lines, "\n");
}

std::string DisplayName(const ItemCatalogEntry& item)
{
    return item.name.empty() ? "(no name)" : item.name;
}

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"%hs\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor(message);
}

// "ref-01.jpg": the owner's added reference images in the order they were added.
std::string ReferenceFileName(int number)
{
    char name[REFERENCE_NAME_CHARS];
    std::snprintf(name, sizeof(name), "ref-%02d.jpg", number);
    return name;
}

std::string GitCommands(const std::string& folder, const std::string& id)
{
    return "git add " + folder + "\n" + "git commit -m \"docs(assets): file item request " + id + "\"\n" +
           "git push";
}
} // namespace

CItemRequestDialog& CItemRequestDialog::GetInstance()
{
    static CItemRequestDialog instance;
    return instance;
}

void CItemRequestDialog::Open(const ItemCatalogEntry& item, const ItemCatalog& catalog)
{
    if (m_stage == Stage::Capturing || m_stage == Stage::Validating)
        return;
    Reset(item, catalog);
    if (m_repo.empty())
        return;
    m_stage = Stage::Form;
    m_openPending = true;
}

// `earlier` is a copy: it may come from the request scan, which a refresh replaces.
void CItemRequestDialog::OpenFollowUp(ItemRequestSummary earlier, const std::string& notes, const ItemCatalog& catalog)
{
    const ItemCatalogEntry* item = earlier.targetKeys.empty() ? nullptr : catalog.FindByKey(earlier.targetKeys.front());
    if (item == nullptr)
        return;
    Open(*item, catalog);
    if (m_stage != Stage::Form)
        return;
    const std::optional<ItemRequestKind> kind = ParseItemKind(earlier.kind);
    const std::optional<ItemRequestKind> setKind = ParseItemKind(earlier.setKind);
    if (kind && (*kind != ItemRequestKind::Set || CanBeSet()))
        m_kind = static_cast<int>(*kind);
    if (setKind)
        m_setKind = static_cast<int>(*setKind);
    std::vector<std::string> details = earlier.details;
    if (!notes.empty())
        details.push_back("After " + earlier.id + " was rejected: " + notes);
    CopyText(m_summary, sizeof(m_summary), earlier.summary);
    CopyText(m_details, sizeof(m_details), Lines(details));
    CopyText(m_keep, sizeof(m_keep), Lines(earlier.keep));
    CopyText(m_avoid, sizeof(m_avoid), Lines(earlier.avoid));
    m_supersedes = earlier.id;
}

void CItemRequestDialog::Reset(const ItemCatalogEntry& item, const ItemCatalog& catalog)
{
    ReleaseConcept();
    m_repo = Editor::Files::RepoRoot().root;
    m_domain = ItemRequestDomain();
    m_targets = {TargetOf(item)};
    CollectSetParts(catalog);
    ReadCheckout();
    FindOpenRequests();
    m_kind = static_cast<int>(CanBeSet() ? ItemRequestKind::Set : ItemRequestKind::Upscale);
    m_setKind = static_cast<int>(ItemRequestKind::Upscale);
    m_priority = static_cast<int>(RequestPriority::Normal);
    m_summary[0] = m_details[0] = m_keep[0] = m_avoid[0] = '\0';
    m_pushAllowed = false;
    m_includeCaptures = true;
    m_supersedes.reset();
    m_references.clear();
    m_conceptFile = m_repo.empty() ? fs::path() : ItemConceptImage(m_repo, item.key);
    std::error_code ec;
    if (!m_conceptFile.empty() && !fs::is_regular_file(m_conceptFile, ec))
        m_conceptFile.clear();
    m_includeConcept = !m_conceptFile.empty();
    LoadConcept();
    m_folder.clear();
    m_validated = false;
    m_validatorReport.clear();
    m_error.clear();
}

ItemRequestTarget CItemRequestDialog::TargetOf(const ItemCatalogEntry& item) const
{
    ItemRequestTarget target{item, {}};
    for (const ItemModel& model : item.models)
        target.modelSha256.push_back(Editor::Files::Sha256Hex(m_repo / Editor::Text::Utf8Path(model.bmd)));
    return target;
}

// The other parts of an armour part's set, in catalog order.
void CItemRequestDialog::CollectSetParts(const ItemCatalog& catalog)
{
    const ItemCatalogEntry& clicked = m_targets.front().item;
    if (!IsArmourGroup(clicked.group) || !clicked.armourSet)
        return;
    for (const std::string& key : clicked.armourSetParts)
    {
        const ItemCatalogEntry* part = catalog.FindByKey(key);
        if (part != nullptr && key != clicked.key && part->armourSet == clicked.armourSet)
            m_targets.push_back(TargetOf(*part));
    }
}

void CItemRequestDialog::ReadCheckout()
{
    const Editor::Git::HeadInfo head = Editor::Git::ReadHead(m_repo);
    m_headCommit = head.commit;
    m_headProblem.clear();
    if (head.commit.empty())
        m_headProblem = "Cannot read the checkout's commit: " + head.error;
    else if (!Editor::Git::IsOriginBranchTip(m_repo, head.commit))
        m_headProblem = "Your checkout is at " + head.commit.substr(0, SHORT_SHA_CHARS) + " (" +
                        (head.branch.empty() ? std::string("detached") : head.branch) +
                        "), which no branch on origin points at. The request records it as base_commit, so push it "
                        "to origin (or file from an up-to-date main) before Codex starts.";
}

void CItemRequestDialog::FindOpenRequests()
{
    m_openRequests.clear();
    for (const ItemRequestTarget& target : m_targets)
    {
        for (const ItemRequestSummary* request : g_ItemRequestWatch.LiveRequestsFor(target.item.key))
        {
            const std::string text = request->id + " (" + request->status + ")";
            if (std::find(m_openRequests.begin(), m_openRequests.end(), text) == m_openRequests.end())
                m_openRequests.push_back(text);
        }
    }
}

void CItemRequestDialog::LoadConcept()
{
    if (m_conceptFile.empty())
        return;
    std::ifstream stream(m_conceptFile, std::ios::binary);
    const std::vector<std::uint8_t> jpeg((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    mu::FramePixels pixels;
    if (!Editor::Capture::DecodeJpeg(jpeg, pixels))
        return;
    const mu::FramePixels thumb = Editor::Capture::DownscaleToWidth(pixels, CONCEPT_THUMB_WIDTH);
    const std::vector<std::uint8_t> rgba = Editor::Capture::ToRgba(thumb);
    m_conceptTexture = mu::GetRenderer().CreateTexture(thumb.width, thumb.height, rgba.data());
    m_conceptAspect = static_cast<float>(thumb.width) / static_cast<float>(std::max<std::uint32_t>(1, thumb.height));
}

void CItemRequestDialog::ReleaseConcept()
{
    if (m_conceptTexture != 0)
        mu::GetRenderer().ReleaseTexture(m_conceptTexture);
    m_conceptTexture = 0;
}

bool CItemRequestDialog::CanBeSet() const
{
    return m_targets.size() > 1;
}

bool CItemRequestDialog::IsSet() const
{
    return static_cast<ItemRequestKind>(m_kind) == ItemRequestKind::Set;
}

ItemOwnerInput CItemRequestDialog::Input() const
{
    ItemOwnerInput input;
    input.kind = static_cast<ItemRequestKind>(m_kind);
    input.setKind = static_cast<ItemRequestKind>(m_setKind);
    input.priority = static_cast<RequestPriority>(m_priority);
    input.summary = Editor::Text::Trim(m_summary);
    input.details = Editor::Text::NonEmptyLines(m_details);
    input.keep = Editor::Text::NonEmptyLines(m_keep);
    input.avoid = Editor::Text::NonEmptyLines(m_avoid);
    input.pushAllowed = m_pushAllowed;
    return input;
}

std::vector<ItemRequestTarget> CItemRequestDialog::Targets() const
{
    if (IsSet() || m_targets.empty())
        return m_targets;
    return {m_targets.front()};
}

std::string CItemRequestDialog::BlockingProblem() const
{
    if (m_headCommit.empty())
        return m_headProblem;
    for (const ItemRequestTarget& target : Targets())
    {
        for (std::size_t i = 0; i < target.item.models.size(); ++i)
        {
            if (target.modelSha256[i].empty())
                return target.item.models[i].bmd + " is missing in the checkout; a request needs every model file.";
        }
    }
    return {};
}

void CItemRequestDialog::Render()
{
    if (m_stage == Stage::Closed)
        return;
    if (m_stage == Stage::Capturing)
        FinishCapturing();
    if (m_stage == Stage::Validating)
        PollValidation();
    if (m_openPending)
    {
        ImGui::OpenPopup(POPUP_TITLE);
        m_openPending = false;
    }

    ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH * g_MuEditorCore.GetUIScale(), 0.0f), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal(POPUP_TITLE, nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;
    g_MuEditorCore.SetHoveringUI(true);
    switch (m_stage)
    {
    case Stage::Form:
        RenderForm();
        break;
    case Stage::Capturing:
    case Stage::Validating:
        RenderCapturing();
        break;
    case Stage::Done:
        RenderDone();
        break;
    case Stage::Closed:
        break;
    }
    ImGui::EndPopup();
}

void CItemRequestDialog::RenderForm()
{
    PollReferencePick();
    RenderHeader();
    RenderKindChoice();
    RenderNotes();
    RenderReferences();
    RenderScope();
    RenderWarnings();
    if (!m_error.empty())
        ImGui::TextColored(COLOR_ERROR, "%s", m_error.c_str());
    RenderValidatorReport();
    ImGui::Separator();
    RenderFormButtons();
}

void CItemRequestDialog::RenderHeader()
{
    const ItemCatalogEntry& item = m_targets.front().item;
    ImGui::Text("%s (%s) - %s, T%d, %d x %d slots", DisplayName(item).c_str(), item.key.c_str(), item.family.c_str(),
                item.tier.value, item.width, item.height);
    if (m_supersedes)
        ImGui::TextDisabled("Follow-up: supersedes %s", m_supersedes->c_str());
    ImGui::Separator();
}

void CItemRequestDialog::RenderKindChoice()
{
    if (ImGui::BeginCombo("Kind", KIND_LABELS[m_kind]))
    {
        for (int kind = 0; kind < ITEM_REQUEST_KIND_COUNT; ++kind)
        {
            const bool setOnly = static_cast<ItemRequestKind>(kind) == ItemRequestKind::Set;
            const ImGuiSelectableFlags flags = setOnly && !CanBeSet() ? ImGuiSelectableFlags_Disabled : 0;
            if (ImGui::Selectable(KIND_LABELS[kind], m_kind == kind, flags))
                m_kind = kind;
        }
        ImGui::EndCombo();
    }
    if (IsSet())
    {
        ImGui::Combo("Every part", &m_setKind, KIND_LABELS, PART_KIND_COUNT);
        std::vector<std::string> parts;
        for (const ItemRequestTarget& target : m_targets)
            parts.push_back(DisplayName(target.item) + " (" + target.item.key + ")");
        ImGui::TextWrapped("Parts: %s", ListOrNone(parts).c_str());
    }
    ImGui::Combo("Priority", &m_priority, PRIORITY_LABELS, IM_ARRAYSIZE(PRIORITY_LABELS));
}

void CItemRequestDialog::RenderNotes()
{
    ImGui::InputTextWithHint("Summary", "One sentence: what should change?", m_summary, sizeof(m_summary));
    const ImVec2 box(-FLT_MIN, ImGui::GetTextLineHeight() * MULTILINE_ROWS);
    ImGui::TextUnformatted("What to change (one point per line)");
    ImGui::InputTextMultiline("##Details", m_details, sizeof(m_details), box);
    ImGui::TextUnformatted("Keep: what must stay as it is");
    ImGui::InputTextMultiline("##Keep", m_keep, sizeof(m_keep), box);
    ImGui::TextUnformatted("Avoid: what the new version must not do");
    ImGui::InputTextMultiline("##Avoid", m_avoid, sizeof(m_avoid), box);
    ImGui::Checkbox("Codex may push its branch and open a PR (never merge)", &m_pushAllowed);
    ImGui::Checkbox("Include captures of the preview (turntable, inventory, worn, +level glow)", &m_includeCaptures);
}

void CItemRequestDialog::RenderReferences()
{
    ImGui::SeparatorText("Reference images");
    RenderConcept();
    for (std::size_t i = 0; i < m_references.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        ImGui::BulletText("%s", m_references[i].label.c_str());
        ImGui::SameLine();
        const bool remove = ImGui::SmallButton("Remove");
        ImGui::PopID();
        if (remove)
        {
            m_references.erase(m_references.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    using Editor::Files::FilePickRequest;
    const bool picking = Editor::Files::IsOpenFilePending(FilePickRequest::ReferenceImage);
    ImGui::BeginDisabled(picking);
    if (ImGui::Button(picking ? "Choosing..." : "Add image..."))
        Editor::Files::RequestOpenFile(FilePickRequest::ReferenceImage);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("JPEG; copied as captures/ref-01.jpg, ref-02.jpg, ...");
}

void CItemRequestDialog::RenderConcept()
{
    if (m_conceptFile.empty())
    {
        ImGui::TextDisabled("No picked concept for this item (tools/item_editor/concepts.py pick).");
        return;
    }
    void* thumb = m_conceptTexture != 0 ? mu::GetRenderer().GetTexturePointer(m_conceptTexture) : nullptr;
    if (thumb != nullptr)
    {
        const float height = CONCEPT_THUMB_SIZE * g_MuEditorCore.GetUIScale();
        ImGui::Image((ImTextureID)(intptr_t)thumb, ImVec2(height * m_conceptAspect, height));
        ImGui::SameLine();
    }
    ImGui::BeginGroup();
    ImGui::Checkbox("Include the picked concept as captures/ref-concept.jpg", &m_includeConcept);
    ImGui::TextDisabled("%s", Editor::Text::PathToUtf8(m_conceptFile.lexically_relative(m_repo)).c_str());
    ImGui::EndGroup();
}

void CItemRequestDialog::PollReferencePick()
{
    using Editor::Files::FilePickRequest;
    using Editor::Files::FilePickState;
    const Editor::Files::FilePickResult pick = Editor::Files::PollOpenFile(FilePickRequest::ReferenceImage);
    if (pick.state == FilePickState::Failed)
        m_error = "The file dialog could not be shown: " + pick.error;
    if (pick.state != FilePickState::Picked)
        return;
    Reference reference;
    reference.label = Editor::Text::PathToUtf8(pick.path.filename());
    std::string error;
    if (!ReadReferenceJpeg(pick.path, Editor::Capture::MAX_CAPTURE_WIDTH, reference.jpeg, error))
    {
        m_error = error;
        return;
    }
    m_error.clear();
    m_references.push_back(std::move(reference));
}

void CItemRequestDialog::RenderScope()
{
    const ItemOwnerInput input = Input();
    const ItemRequestScope scope = ComputeItemScope(PartKind(input), Targets());
    ImGui::SeparatorText("Scope");
    ImGui::TextWrapped("Codex may replace: %s", ListOrNone(scope.ownedFiles).c_str());
    if (!scope.frozenTextures.empty())
        ImGui::TextWrapped("Frozen (shared with other items or models): %s",
                           ListOrNone(scope.frozenTextures).c_str());
}

void CItemRequestDialog::RenderWarnings()
{
    ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
    if (!m_headProblem.empty())
        ImGui::TextWrapped("%s", m_headProblem.c_str());
    for (const ItemRequestTarget& target : Targets())
    {
        for (std::size_t i = 0; i < target.item.models.size(); ++i)
        {
            const ItemModel& model = target.item.models[i];
            if (!target.modelSha256[i].empty() && target.modelSha256[i] != model.originalSha256)
                ImGui::TextWrapped("%s in your checkout differs from catalog.json (an uncommitted change, or the "
                                   "catalog is out of date). Commit or restore it, or rebuild the catalog, first.",
                                   model.bmd.c_str());
        }
    }
    if (!m_openRequests.empty())
        ImGui::TextWrapped("Open request for this item already: %s. File another one only for a different change.",
                           ListOrNone(m_openRequests).c_str());
    const ItemRequestKind part = PartKind(Input());
    const bool texturesOnly = part == ItemRequestKind::Upscale || part == ItemRequestKind::Repaint;
    if (texturesOnly && ComputeItemScope(part, Targets()).ownedFiles.empty())
        ImGui::TextWrapped("Every texture of this item is shared with other items or models, so an %s could change "
                           "nothing. Choose remodel or redesign, or a set.",
                           ItemKindName(part));
    ImGui::PopStyleColor();
}

void CItemRequestDialog::RenderValidatorReport()
{
    if (m_validatorReport.empty())
        return;
    ImGui::TextColored(COLOR_ERROR, "validate_request.py rejected the request; nothing was kept:");
    ImGui::InputTextMultiline("##ValidatorReport", m_validatorReport.data(), m_validatorReport.size() + 1,
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * REPORT_ROWS), ImGuiInputTextFlags_ReadOnly);
}

void CItemRequestDialog::RenderFormButtons()
{
    const std::string blocking = BlockingProblem();
    if (!blocking.empty())
        ImGui::TextColored(COLOR_ERROR, "%s", blocking.c_str());
    ImGui::BeginDisabled(Editor::Text::Trim(m_summary).empty() || !blocking.empty());
    if (ImGui::Button("Create"))
        Create();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
        Close();
}

void CItemRequestDialog::RenderCapturing()
{
    if (m_stage == Stage::Validating)
    {
        ImGui::TextUnformatted("Checking the request with validate_request.py...");
        return;
    }
    const int count = std::max(1, m_capture.ShotCount());
    const float progress = static_cast<float>(m_capture.ShotNumber()) / static_cast<float>(count);
    ImGui::Text("Capturing the preview: %s (%d of %d)", m_capture.CurrentShot(), m_capture.ShotNumber() + 1, count);
    ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0.0f));
    if (ImGui::Button("Cancel"))
    {
        m_capture.Cancel();
        m_stage = Stage::Form;
    }
}

void CItemRequestDialog::RenderDone()
{
    const std::string folder = RequestFolderPath(m_domain, m_draft.id);
    ImGui::Text("Request written:");
    ImGui::TextWrapped("%s", Editor::Text::PathToUtf8(m_folder).c_str());
    if (m_validated)
        ImGui::TextColored(COLOR_OK, "validate_request.py: OK");
    else
        ImGui::TextColored(Editor::StatusLine::WarningColor(), "Not validated (no python3 found); run %s/%s on it.",
                           RequestsFolderPath(m_domain).c_str(), VALIDATOR_SCRIPT);
    ImGui::Separator();
    ImGui::TextWrapped("Codex only sees it once it is on origin/main. Commit just this folder on main and push it:");
    const std::string commands = GitCommands(folder, m_draft.id);
    ImGui::TextDisabled("%s", commands.c_str());
    if (ImGui::Button("Copy commands"))
        ImGui::SetClipboardText(commands.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Open folder"))
    {
        std::string error;
        if (!Editor::Files::OpenWithSystem(m_folder, error))
            m_error = error;
    }
    ImGui::SameLine();
    if (ImGui::Button("Close"))
        Close();
    ImGui::TextWrapped("The coordinator then assigns it to a worker; see %s/README.md.",
                       RequestsFolderPath(m_domain).c_str());
    if (!m_error.empty())
        ImGui::TextColored(COLOR_ERROR, "%s", m_error.c_str());
}

void CItemRequestDialog::Close()
{
    m_capture.Cancel();
    ReleaseConcept();
    m_stage = Stage::Closed;
    ImGui::CloseCurrentPopup();
}

void CItemRequestDialog::Create()
{
    m_error.clear();
    m_validatorReport.clear();
    if (!BuildDraft(m_error) || !BuildReferences(m_error))
        return;
    if (!m_includeCaptures)
    {
        Write();
        return;
    }
    const ItemCatalogEntry& item = m_targets.front().item;
    const bool canBeExcellent = item.badges.excellent;
    m_capture.Start(item.Type(), Editor::Preview::PlanItemCaptures(canBeExcellent),
                    m_headCommit.substr(0, SHORT_SHA_CHARS));
    m_stage = Stage::Capturing;
}

bool CItemRequestDialog::BuildDraft(std::string& error)
{
    error = BlockingProblem();
    if (!error.empty())
        return false;
    ItemRequestDraft draft;
    const Timestamp now = CurrentTimestamp();
    draft.created = now.dateTime;
    draft.domain = m_domain;
    draft.baseCommit = m_headCommit;
    draft.supersedes = m_supersedes;
    draft.input = Input();
    draft.targets = Targets();
    draft.id = MakeRequestId(now.date, draft.targets.front().item.key, MakeSlug(draft.input.summary),
                             ExistingRequestIds(m_repo, m_domain));
    m_draft = std::move(draft);
    m_images = {};
    return true;
}

bool CItemRequestDialog::BuildReferences(std::string& error)
{
    if (m_includeConcept && !m_conceptFile.empty())
    {
        std::vector<std::uint8_t> jpeg;
        if (!ReadReferenceJpeg(m_conceptFile, Editor::Capture::MAX_CAPTURE_WIDTH, jpeg, error))
            return false;
        m_draft.referenceImages.push_back(CONCEPT_REFERENCE);
        m_images.references.push_back(std::move(jpeg));
    }
    for (std::size_t i = 0; i < m_references.size(); ++i)
    {
        m_draft.referenceImages.push_back(ReferenceFileName(static_cast<int>(i) + 1));
        m_images.references.push_back(m_references[i].jpeg);
    }
    return true;
}

void CItemRequestDialog::FinishCapturing()
{
    m_capture.Step();
    if (m_capture.HasFailed())
    {
        m_error = m_capture.Error();
        m_stage = Stage::Form;
        return;
    }
    if (!m_capture.IsDone())
        return;
    m_draft.captures = m_capture.Captures();
    m_images.captures = m_capture.Jpegs();
    Write();
}

void CItemRequestDialog::Write()
{
    std::string error;
    if (!WriteItemRequestFolder(m_repo, m_draft, m_images, m_folder, error))
    {
        m_error = "Nothing was written: " + error;
        m_stage = Stage::Form;
        return;
    }
    Log("[Items] Filed item request " + Editor::Text::PathToUtf8(m_folder));
    g_ItemRequestWatch.Refresh();
    const fs::path script = RequestsDir(m_repo, m_domain) / VALIDATOR_SCRIPT;
    m_validation = Editor::PythonTool::RunInBackground(script, {Editor::Text::PathToUtf8(m_folder)});
    m_stage = Stage::Validating;
}

void CItemRequestDialog::PollValidation()
{
    using namespace std::chrono_literals;
    if (!m_validation.valid() || m_validation.wait_for(0s) != std::future_status::ready)
        return;
    const Editor::PythonTool::Result result = m_validation.get();
    m_validated = result.started && result.exitCode == 0;
    if (!result.started || m_validated)
    {
        Log("[Items] validate_request.py: " + std::string(m_validated ? "OK" : "not run (no python3)"));
        m_stage = Stage::Done;
        return;
    }
    // Keep nothing the art builder could pick up by mistake; the form keeps the owner's text.
    std::error_code ec;
    fs::remove_all(m_folder, ec);
    g_ItemRequestWatch.Refresh();
    m_validatorReport = result.output;
    Log("[Items] validate_request.py rejected " + m_draft.id + "; the folder was removed:\n" + result.output);
    m_stage = Stage::Form;
}

#endif // _EDITOR
