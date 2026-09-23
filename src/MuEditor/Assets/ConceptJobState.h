#pragma once

#ifdef _EDITOR

#include "ConceptPlan.h" // Cost, PlanReason

#include <json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A background `concepts.py run` (or `refs`) as the Item Editor shows it, built
// from the JSON Lines events it writes with --json-progress and its exit code
// (assets-work/Items/concepts/README.md, "Streaming commands" and "Exit codes").
// Pure data: the caller feeds lines and the time; no process or UI here.
namespace Editor::Concepts
{
enum class JobItemState
{
    Waiting,    // not started yet
    Generating, // a request is on the wire
    Retrying,   // waiting to send a request again (rate limit, server error)
    Done,
    Failed,
    Cancelled, // not sent because the run was cancelled
};
const char* JobItemStateLabel(JobItemState state);

struct JobItem
{
    std::string key; // the concept key the events name
    std::string name;
    JobItemState state = JobItemState::Waiting;
    int requestsStarted = 0;
    int requestsDone = 0;
    int requestsFailed = 0;
    int images = 0;         // images written so far
    std::string message;    // the last failure or retry reason
    double retryDelay = 0;  // seconds, for Retrying
    double retrySince = 0;  // the caller's clock when the retry event arrived
};

enum class JobPhase
{
    Starting, // no event yet
    Running,
    Finished,             // everything done (or nothing to do)
    FinishedWithFailures, // exit 4: `run --resume` retries the failed requests
    Cancelled,            // exit 130: `resume` continues it
    Refused,              // exit 3: above a cap
    Busy,                 // exit 6: another process holds the batch or the refs folder
    NoApiKey,             // exit 5
    Failed,               // any other error
};
const char* JobPhaseLabel(JobPhase phase);
// The process has ended (or will write no more events).
bool IsFinal(JobPhase phase);

struct ConceptJobState
{
    JobPhase phase = JobPhase::Starting;
    std::string batch;
    std::string dir;
    std::string sheet;
    std::vector<JobItem> items;
    int requests = 0;
    int images = 0;
    Cost estimated;
    std::optional<Cost> actual; // from the API's usage, when the run reported it
    int done = 0;
    int failed = 0;
    int notStarted = 0;
    std::vector<std::string> resume; // the arguments that continue a cancelled run
    std::string message;             // an error, refusal or cancel reason
    std::vector<PlanReason> reasons; // refused
    bool cancelRequested = false;    // the editor sent SIGTERM; in-flight requests still finish
    std::optional<int> exitCode;
    int rendered = 0; // refs: reference images rendered
    int cached = 0;   // refs: already up to date

    // Adds an item in the Waiting state unless it is listed already.
    JobItem& Item(const std::string& key);
    // Done + failed items against all items, 0..1.
    float Progress() const;
    bool CanResume() const;
};

// Applies one event object; `now` is the caller's clock in seconds (for retry countdowns).
void ApplyEvent(ConceptJobState& state, const nlohmann::json& event, double now);
// Parses and applies one stdout line. False when it is not a protocol event
// (ignored; stdout carries nothing else).
bool ApplyLine(ConceptJobState& state, std::string_view line, double now);
// The process ended with `exitCode`: settles the phase when the last event did
// not (killed, an argument error before any event, busy).
void ApplyExit(ConceptJobState& state, int exitCode);

// The seconds left of an item's retry wait at `now` (0 when not retrying).
double RetrySecondsLeft(const JobItem& item, double now);
} // namespace Editor::Concepts

#endif // _EDITOR
