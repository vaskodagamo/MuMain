#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptJob.h"

#include "ConceptTool.h"

#include "Assets/ConceptCommands.h"
#include "Core/PythonTool.h"
#include "UI/Console/MuEditorConsoleUI.h"

#include <chrono>

using Editor::Concepts::ConceptJobState;
using Editor::Concepts::JobItemState;
using Editor::Concepts::JobPhase;

namespace
{
constexpr std::size_t LOG_TAIL_CHARS = 16 * 1024;

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"%hs\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor(message);
}
} // namespace

CConceptJob& CConceptJob::GetInstance()
{
    static CConceptJob instance;
    return instance;
}

bool CConceptJob::IsRunning() const
{
    return m_process && m_process->IsRunning();
}

double CConceptJob::Now() const
{
    using Seconds = std::chrono::duration<double>;
    return std::chrono::duration_cast<Seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool CConceptJob::Start(Kind kind, std::string title, std::vector<std::string> arguments,
                        const std::vector<Item>& items, std::string& error)
{
    if (IsRunning())
    {
        error = "Another concepts job is still running; wait for it or cancel it first.";
        return false;
    }
    const std::filesystem::path& repo = Editor::ItemEditor::ConceptTool::Repo();
    if (repo.empty())
    {
        error = "No repository checkout: concepts need one.";
        return false;
    }
    auto process = std::make_unique<Editor::ChildProcess>();
    if (!Editor::PythonTool::StartScript(Editor::Concepts::ScriptPath(repo), arguments, *process, error))
    {
        error = "python3 could not be started: " + error;
        return false;
    }
    m_process = std::move(process);
    m_kind = kind;
    m_title = std::move(title);
    m_items = items;
    m_state = {};
    m_log.clear();
    SeedItems(items);
    m_hasJob = true;
    ++m_version;
    Log("[Concepts] Started: " + m_title + " (python: " + Editor::PythonTool::Interpreter() + ")");
    return true;
}

void CConceptJob::SeedItems(const std::vector<Item>& items)
{
    for (const auto& [key, name] : items)
        m_state.Item(key).name = name;
}

void CConceptJob::Poll()
{
    if (!m_process || !m_process->IsStarted() || m_state.exitCode)
        return;
    std::vector<std::string> lines;
    std::string errors;
    m_process->Poll(lines, errors);
    AppendLog(errors);
    const double now = Now();
    for (const std::string& line : lines)
    {
        if (Editor::Concepts::ApplyLine(m_state, line, now))
            ++m_version;
    }
    if (const std::optional<int> exitCode = m_process->ExitCode())
    {
        Editor::Concepts::ApplyExit(m_state, *exitCode);
        OnEnded();
    }
}

void CConceptJob::OnEnded()
{
    ++m_version;
    ++m_endedVersion;
    std::string summary = "[Concepts] " + m_title + ": " + Editor::Concepts::JobPhaseLabel(m_state.phase);
    if (!m_state.message.empty())
        summary += " - " + m_state.message;
    if (m_state.actual)
        summary += " (actual " + Editor::Concepts::FormatDollars(m_state.actual->total) + ", estimated " +
                   Editor::Concepts::FormatDollars(m_state.estimated.total) + ")";
    Log(summary);
}

void CConceptJob::Cancel()
{
    if (!IsRunning() || m_state.cancelRequested)
        return;
    m_state.cancelRequested = m_process->Terminate();
    ++m_version;
    Log("[Concepts] Cancelling " + m_title + ": no new requests; requests already sent finish.");
}

bool CConceptJob::Resume(std::string& error)
{
    if (!m_state.CanResume())
    {
        error = "Nothing to resume.";
        return false;
    }
    const std::vector<std::string> resume = m_state.resume;
    const std::vector<Item> items = m_items;
    std::vector<std::string> arguments = Editor::Concepts::ResumeArgs(
        resume, Editor::ItemEditor::ConceptTool::Repo(), Editor::ItemEditor::ConceptTool::TestApiBase());
    ConceptJobState before = m_state;
    if (!Start(m_kind, m_title + " (resumed)", std::move(arguments), items, error))
        return false;
    // Items already done stay done; the rest wait again.
    for (const Editor::Concepts::JobItem& item : before.items)
    {
        Editor::Concepts::JobItem& now = m_state.Item(item.key);
        if (item.state == JobItemState::Done)
            now = item;
    }
    return true;
}

void CConceptJob::Dismiss()
{
    if (IsRunning())
        return;
    m_hasJob = false;
    m_process.reset();
    ++m_version;
}

void CConceptJob::Shutdown()
{
    if (IsRunning())
        m_process->Terminate();
    m_process.reset();
}

void CConceptJob::AppendLog(const std::string& text)
{
    if (text.empty())
        return;
    m_log += text;
    if (m_log.size() > LOG_TAIL_CHARS)
        m_log.erase(0, m_log.size() - LOG_TAIL_CHARS);
}

#endif // _EDITOR
