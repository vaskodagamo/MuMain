#pragma once

#ifdef _EDITOR

#include "ItemCaptureRun.h"

#include "Assets/ItemCatalog.h"
#include "Assets/ItemRequest.h"
#include "Assets/ItemRequestFolder.h"
#include "Assets/ItemRequestScan.h"
#include "Core/PythonTool.h"

#include <cstdint>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <vector>

// "Ask Codex...": the dialog in which the owner asks the art builder to rework an
// item (assets-work/Items/requests/README.md). Create takes the clean captures
// from the Browse preview (ItemCaptureRun), writes the request folder
// (request.json, brief.md, captures/, the picked concept and other reference
// images as captures/ref-*.jpg) in one go, runs validate_request.py on it when
// Python is there, and shows the git commands that hand it to Codex. The editor
// never commits or pushes.
class CItemRequestDialog
{
public:
    static CItemRequestDialog& GetInstance();

    // Opens the dialog for `item` (selected in Browse; its preview takes the captures).
    void Open(const Editor::Assets::ItemCatalogEntry& item, const Editor::Assets::ItemCatalog& catalog);
    // Opens it for a follow-up of `earlier` (after a rejection): the same kind and
    // notes, `notes` added to the details, and the new request supersedes it.
    void OpenFollowUp(Editor::Assets::ItemRequestSummary earlier, const std::string& notes,
                      const Editor::Assets::ItemCatalog& catalog);

    // Draws the dialog while it is open and advances a running capture. Call once
    // per frame inside the Item Editor window, after the Browse tab.
    void Render();

private:
    enum class Stage
    {
        Closed,
        Form,
        Capturing,  // Create was pressed; the preview is being captured
        Validating, // the folder is written; validate_request.py runs
        Done,
    };

    // A reference image the owner added, already read (and scaled down if needed).
    struct Reference
    {
        std::string label;
        std::vector<std::uint8_t> jpeg;
    };

    CItemRequestDialog() = default;

    void Reset(const Editor::Assets::ItemCatalogEntry& item, const Editor::Assets::ItemCatalog& catalog);
    void CollectSetParts(const Editor::Assets::ItemCatalog& catalog);
    Editor::Assets::ItemRequestTarget TargetOf(const Editor::Assets::ItemCatalogEntry& item) const;
    void ReadCheckout();
    void FindOpenRequests();
    void LoadConcept();
    void ReleaseConcept();

    void RenderForm();
    void RenderHeader();
    void RenderKindChoice();
    void RenderNotes();
    void RenderReferences();
    void RenderConcept();
    void PollReferencePick();
    void RenderScope();
    void RenderWarnings();
    void RenderValidatorReport();
    void RenderFormButtons();
    void RenderCapturing();
    void RenderDone();
    void Close();

    void Create();
    bool BuildDraft(std::string& error);
    bool BuildReferences(std::string& error);
    void FinishCapturing();
    void Write();
    void PollValidation();

    bool CanBeSet() const;
    bool IsSet() const;
    Editor::Assets::ItemOwnerInput Input() const;
    std::vector<Editor::Assets::ItemRequestTarget> Targets() const;
    // Why Create cannot run (a model file is missing ...); empty when it can.
    std::string BlockingProblem() const;

    Stage m_stage = Stage::Closed;
    bool m_openPending = false;
    std::filesystem::path m_repo;
    Editor::Assets::RequestDomain m_domain;

    // The clicked item first; for an armour part, the other parts of its set after it.
    std::vector<Editor::Assets::ItemRequestTarget> m_targets;
    std::string m_headCommit;
    std::string m_headProblem;
    std::vector<std::string> m_openRequests; // "<id> (<status>)" naming a target

    int m_kind = 0;    // ItemRequestKind
    int m_setKind = 0; // ItemRequestKind of every part of a set
    int m_priority = static_cast<int>(Editor::Assets::RequestPriority::Normal);
    char m_summary[256] = {};
    char m_details[2048] = {};
    char m_keep[1024] = {};
    char m_avoid[1024] = {};
    bool m_pushAllowed = false;
    bool m_includeCaptures = true;
    bool m_includeConcept = false;
    std::optional<std::string> m_supersedes;

    std::filesystem::path m_conceptFile; // empty when the item has no picked concept
    std::uint32_t m_conceptTexture = 0;
    float m_conceptAspect = 1.0f;
    std::vector<Reference> m_references;

    Editor::Assets::ItemRequestDraft m_draft;
    Editor::Assets::ItemRequestImages m_images;
    CItemCaptureRun m_capture;
    std::future<Editor::PythonTool::Result> m_validation;
    std::filesystem::path m_folder;
    bool m_validated = false; // validate_request.py ran on the folder and said OK
    std::string m_validatorReport;
    std::string m_error;
};

#define g_ItemRequestDialog CItemRequestDialog::GetInstance()

#endif // _EDITOR
