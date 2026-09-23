#pragma once

#ifdef _EDITOR

#include "Assets/ConceptJobState.h"
#include "Core/ChildProcess.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// The one concepts.py run (or refs render) the Item Editor has going in the
// background: started from the Generate / Refine dialogs, followed through its
// JSON Lines events, cancelled with SIGTERM (requests already sent finish and are
// kept), resumed after a cancel or failures. It lives on when the dialog that
// started it closes; Poll() runs every frame from the editor core, also while the
// Item Editor window is closed. One job at a time: Start() refuses while one runs.
class CConceptJob
{
public:
    enum class Kind
    {
        Generate,
        Refine,
        References,
    };

    // An item the job works on: its concept key and a name to show.
    using Item = std::pair<std::string, std::string>;

    static CConceptJob& GetInstance();

    // Starts `concepts.py <arguments>`; false (and why in `error`) while another job runs.
    bool Start(Kind kind, std::string title, std::vector<std::string> arguments, const std::vector<Item>& items,
               std::string& error);
    // Reads the child's new events; call once per frame.
    void Poll();
    // Asks the run to stop: no new request starts, requests on the wire finish.
    void Cancel();
    // Continues a cancelled run, or retries the failed requests of a finished one.
    bool Resume(std::string& error);
    // Forgets a finished job (hides the panel).
    void Dismiss();
    // The editor is closing: stops a running job the same way Cancel() does.
    void Shutdown();

    bool IsRunning() const;
    bool HasJob() const { return m_hasJob; }
    Kind JobKind() const { return m_kind; }
    const std::string& Title() const { return m_title; }
    const Editor::Concepts::ConceptJobState& State() const { return m_state; }
    // The last lines the tool wrote to stderr (its human log).
    const std::string& LogTail() const { return m_log; }
    // Seconds on the editor's clock (for retry countdowns).
    double Now() const;

    // Changes with every event (panels showing the batch reload).
    std::uint64_t Version() const { return m_version; }
    // Changes when a job ends (listings and plans are read again).
    std::uint64_t EndedVersion() const { return m_endedVersion; }

private:
    CConceptJob() = default;
    void SeedItems(const std::vector<Item>& items);
    void AppendLog(const std::string& text);
    void OnEnded();

    std::unique_ptr<Editor::ChildProcess> m_process;
    Editor::Concepts::ConceptJobState m_state;
    Kind m_kind = Kind::Generate;
    std::string m_title;
    std::vector<Item> m_items;
    std::string m_log;
    bool m_hasJob = false;
    std::uint64_t m_version = 0;
    std::uint64_t m_endedVersion = 0;
};

#define g_ConceptJob CConceptJob::GetInstance()

#endif // _EDITOR
