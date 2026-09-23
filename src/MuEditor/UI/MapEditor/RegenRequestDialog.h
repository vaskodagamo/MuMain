#pragma once

#ifdef _EDITOR

#include "Assets/AssetCatalog.h"
#include "Assets/RegenRequest.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

class OBJECT;

// "Flag for regeneration...": a dialog in which the owner describes what the art
// builder should change about a model. Create writes the request folder
// (assets-work/World{N}/requests/<id>/: request.json, brief.md and, if wanted, a
// capture of the current view) as assets-work/World1/requests/README.md defines
// it, and runs no other tool. Opened from the Assets tab and the Objects tab.
class CRegenRequestDialog
{
public:
    struct FiledRequest
    {
        std::string model;
        std::string id;
    };

    static CRegenRequestDialog& GetInstance();

    // Opens the dialog for `model` of `catalog`. `picked` is the placed instance the
    // owner selected, or nullptr.
    void Open(int world, const Editor::Assets::Catalog& catalog, const std::string& model, const OBJECT* picked);

    // Draws the dialog while it is open and finishes a started capture. Call once
    // per frame inside the Map Editor window.
    void Render();

    // The request written since the last call, if any.
    std::optional<FiledRequest> TakeFiledRequest();

private:
    enum class Stage
    {
        Closed,
        Form,
        Capturing, // Create was pressed; waiting for the frame without the editor
        Done,
    };

    CRegenRequestDialog() = default;

    void ReadCheckout();
    void RenderForm();
    void RenderOwnerFields();
    void RenderScopePreview();
    void RenderWarnings();
    void RenderDone();
    void Create();
    void FinishCapture();
    bool BuildDraft(std::string& error);
    Editor::Assets::CaptureInfo CurrentView() const;
    void Write(const std::vector<std::uint8_t>& jpeg);
    bool IncludesPartners() const;
    std::vector<Editor::Assets::RequestTarget> Targets() const;
    // The shared, frozen and owned files for the current kind and targets; recomputed
    // only when those change.
    const Editor::Assets::RequestScope& Scope();
    Editor::Assets::RequestKind Kind() const;

    Stage m_stage = Stage::Closed;
    bool m_openPending = false;

    int m_world = 0;
    std::string m_worldName;
    // The clicked model first (with the picked instance), then the in-scope models
    // that share its textures, each with the SHA-256 of its BMD in the checkout.
    std::vector<Editor::Assets::RequestTarget> m_targets;
    std::set<std::string> m_takenNames; // for the new-variant name
    std::string m_headCommit;
    std::string m_headProblem; // why base_commit may not work for the worker
    // EncTerrain{N}.obj in the checkout equals the file at HEAD, so a picked
    // instance's record number there is the worker's too.
    bool m_objectFileCommitted = false;

    int m_scopeKind = -1;
    bool m_scopeWithPartners = false;
    Editor::Assets::RequestScope m_scope;

    int m_kind = static_cast<int>(Editor::Assets::RequestKind::Repaint);
    int m_priority = static_cast<int>(Editor::Assets::RequestPriority::Normal);
    char m_summary[256] = {};
    char m_details[2048] = {};
    char m_keep[1024] = {};
    char m_avoid[1024] = {};
    bool m_includeView = true;
    bool m_includePartners = false;
    bool m_pushAllowed = false;

    Editor::Assets::RequestDraft m_draft; // while capturing
    std::filesystem::path m_folder;       // the folder written
    std::string m_error;
    std::optional<FiledRequest> m_filed;
};

#define g_RegenRequestDialog CRegenRequestDialog::GetInstance()

#endif // _EDITOR
