#pragma once

#ifdef _EDITOR

#include "Assets/ItemCatalog.h"
#include "Assets/ItemRequestScan.h"
#include "Core/PythonTool.h"

#include <future>
#include <string>

// The Item Editor's Requests tab: every folder under assets-work/Items/requests/
// with its item, kind, status, the owner's verdict, date, worker branch and
// whether a delivery is there. Clicking a row selects its item (as in Browse and
// the Stats table). For the selected request the owner can withdraw it (open or
// claimed), accept it or reject it with notes (delivered: owner-decision.json),
// file a follow-up after a rejection, open its folder or validate it; each change
// shows the git commands that hand it on. The editor never commits or pushes.
// Compare shows a delivered item side by side with the checkout's files in Browse.
class CItemRequestsTab
{
public:
    // `selectedType` is the Item Editor's selected item; `showInBrowse` is set when
    // the owner wants to see it there (or files a follow-up, which captures it).
    void Render(int& selectedType, bool& showInBrowse, const Editor::Assets::ItemCatalog* catalog);

private:
    void RenderToolbar();
    void RenderTable(int& selectedType, const Editor::Assets::ItemCatalog* catalog);
    void RenderRow(const Editor::Assets::ItemRequestSummary& request, int& selectedType,
                   const Editor::Assets::ItemCatalog* catalog);
    void RenderSelected(bool& showInBrowse, const Editor::Assets::ItemCatalog* catalog);
    void RenderWithdraw(const Editor::Assets::ItemRequestSummary& request);
    void RenderVerdict(const Editor::Assets::ItemRequestSummary& request, bool& showInBrowse);
    void RenderCompare(const Editor::Assets::ItemRequestSummary& request, bool& showInBrowse);
    void RenderFollowUp(const Editor::Assets::ItemRequestSummary& request, bool& showInBrowse,
                        const Editor::Assets::ItemCatalog* catalog);
    void RenderResult();
    void Select(const Editor::Assets::ItemRequestSummary& request, int& selectedType,
                const Editor::Assets::ItemCatalog* catalog);

    void Withdraw(const Editor::Assets::ItemRequestSummary& request);
    void Decide(const Editor::Assets::ItemRequestSummary& request, const char* verdict);
    void Validate(const Editor::Assets::ItemRequestSummary& request);
    void PollValidation();

    std::string m_selectedId;
    // An action changed a request folder: read them again at the start of the next
    // frame, never while this frame still draws the rows it read.
    bool m_rescanPending = false;
    bool m_confirmWithdraw = false;
    char m_notes[1024] = {};
    std::string m_result;   // what the last action did
    std::string m_error;
    std::string m_commands; // the git commands that hand the last change on
    std::future<Editor::PythonTool::Result> m_validation;
    std::string m_validatingId;
};

#endif // _EDITOR
