#include "ConceptJobState.h"

#ifdef _EDITOR

#include "JsonFields.h"

#include <algorithm>

using nlohmann::json;

namespace Editor::Concepts
{
namespace
{
using Assets::Json::Array;
using Assets::Json::Bool;
using Assets::Json::Int;
using Assets::Json::Member;
using Assets::Json::Number;
using Assets::Json::Text;
using Assets::Json::TextList;

constexpr int PROTOCOL_VERSION = 1;

// concepts.py exit codes (README "Exit codes").
constexpr int EXIT_OK = 0;
constexpr int EXIT_USAGE = 2;
constexpr int EXIT_REFUSED = 3;
constexpr int EXIT_FAILED_REQUESTS = 4;
constexpr int EXIT_NO_API_KEY = 5;
constexpr int EXIT_BUSY = 6;
constexpr int EXIT_CANCELLED = 130;
constexpr int EXIT_INTERPRETER_MISSING = 127; // the shell's "command not found"

bool InFlight(const JobItem& item)
{
    return item.requestsStarted > item.requestsDone + item.requestsFailed;
}

// After a request ended: done, failed, or still generating when another is on the wire.
void SettleItem(JobItem& item)
{
    if (InFlight(item))
        item.state = JobItemState::Generating;
    else if (item.requestsFailed > 0)
        item.state = JobItemState::Failed;
    else
        item.state = JobItemState::Done;
}

JobPhase PhaseOfExit(int exitCode)
{
    switch (exitCode)
    {
    case EXIT_OK:
        return JobPhase::Finished;
    case EXIT_REFUSED:
        return JobPhase::Refused;
    case EXIT_FAILED_REQUESTS:
        return JobPhase::FinishedWithFailures;
    case EXIT_NO_API_KEY:
        return JobPhase::NoApiKey;
    case EXIT_BUSY:
        return JobPhase::Busy;
    case EXIT_CANCELLED:
        return JobPhase::Cancelled;
    default:
        return JobPhase::Failed;
    }
}

std::string ExitMessage(int exitCode)
{
    switch (exitCode)
    {
    case EXIT_OK:
        return "concepts.py ended without its final report.";
    case EXIT_USAGE:
        return "concepts.py rejected the arguments (exit 2); see the log.";
    case EXIT_NO_API_KEY:
        return "No OpenAI API key found (neither OPENAI_API_KEY nor the Keychain item).";
    case EXIT_BUSY:
        return "Busy: another concepts.py run holds this batch or the reference folder.";
    case EXIT_INTERPRETER_MISSING:
        return "python3 could not be started.";
    default:
        if (exitCode < 0)
            return "concepts.py was stopped by a signal.";
        return "concepts.py ended with exit " + std::to_string(exitCode) + ".";
    }
}

void OnStarted(ConceptJobState& state, const json& event)
{
    state.phase = JobPhase::Running;
    state.batch = Text(event, "batch");
    state.dir = Text(event, "dir");
    state.requests = Int(event, "requests", 0);
    state.images = Int(event, "images", 0);
    state.estimated = ParseCost(Member(event, "estimate"));
}

void OnRequestStarted(ConceptJobState& state, const json& event)
{
    JobItem& item = state.Item(Text(event, "key"));
    ++item.requestsStarted;
    item.state = JobItemState::Generating;
}

void OnRetry(ConceptJobState& state, const json& event, double now)
{
    JobItem& item = state.Item(Text(event, "key"));
    item.state = JobItemState::Retrying;
    item.retryDelay = Number(event, "delay_s", 0.0);
    item.retrySince = now;
    item.message = Text(event, "reason");
}

void OnRequestDone(ConceptJobState& state, const json& event)
{
    JobItem& item = state.Item(Text(event, "key"));
    item.requestsStarted = std::max(item.requestsStarted, item.requestsDone + item.requestsFailed + 1); // refs: cached
    ++item.requestsDone;
    item.images += static_cast<int>(Array(event, "variants").size());
    SettleItem(item);
}

void OnRequestFailed(ConceptJobState& state, const json& event)
{
    JobItem& item = state.Item(Text(event, "key"));
    item.requestsStarted = std::max(item.requestsStarted, item.requestsDone + item.requestsFailed + 1);
    ++item.requestsFailed;
    item.message = Text(event, "error");
    SettleItem(item);
}

void OnRequestCancelled(ConceptJobState& state, const json& event)
{
    JobItem& item = state.Item(Text(event, "key"));
    if (item.state != JobItemState::Done)
        item.state = JobItemState::Cancelled;
}

void ReadEndCounts(ConceptJobState& state, const json& event)
{
    state.batch = event.contains("batch") ? Text(event, "batch") : state.batch;
    state.dir = event.contains("dir") ? Text(event, "dir") : state.dir;
    state.sheet = Text(event, "sheet");
    state.done = Int(event, "done", 0);
    state.failed = Int(event, "failed", 0);
    state.notStarted = Int(event, "not_started", 0);
    state.rendered = Int(event, "rendered", 0);
    state.cached = Int(event, "cached", 0);
    const json& cost = Member(event, "cost");
    if (cost.contains("estimated"))
        state.estimated = ParseCost(Member(cost, "estimated"));
    if (Int(cost, "requests_with_usage", 0) > 0)
        state.actual = ParseCost(Member(cost, "actual"));
}

void OnFinished(ConceptJobState& state, const json& event)
{
    ReadEndCounts(state, event);
    state.phase = state.failed > 0 ? JobPhase::FinishedWithFailures : JobPhase::Finished;
    if (Bool(event, "nothing_to_do", false))
        state.message = "Nothing to do: every image of the batch exists.";
    if (state.failed > 0)
        state.resume = {"run", "--resume", state.batch, "--yes"};
}

void OnCancelled(ConceptJobState& state, const json& event)
{
    ReadEndCounts(state, event);
    state.phase = JobPhase::Cancelled;
    state.message = "Cancelled (" + Text(event, "reason") + ")";
    state.resume = TextList(event, "resume");
    for (JobItem& item : state.items)
    {
        if (item.state == JobItemState::Waiting || item.state == JobItemState::Retrying)
            item.state = JobItemState::Cancelled;
    }
}

void OnRefused(ConceptJobState& state, const json& event)
{
    state.phase = JobPhase::Refused;
    state.estimated = ParseCost(Member(event, "estimate"));
    for (const json& reason : Array(event, "reasons"))
        state.reasons.push_back({Text(reason, "code"), Text(reason, "message"), TextList(reason, "keys")});
    state.message = "Refused: the estimate is above a cap.";
}

void OnError(ConceptJobState& state, const json& event)
{
    const int exitCode = Int(event, "exit", 1);
    state.phase = PhaseOfExit(exitCode);
    if (state.phase == JobPhase::Finished)
        state.phase = JobPhase::Failed;
    state.message = Text(event, "message");
}
} // namespace

const char* JobItemStateLabel(JobItemState state)
{
    switch (state)
    {
    case JobItemState::Waiting:
        return "waiting";
    case JobItemState::Generating:
        return "generating";
    case JobItemState::Retrying:
        return "retrying";
    case JobItemState::Done:
        return "done";
    case JobItemState::Failed:
        return "failed";
    case JobItemState::Cancelled:
        return "cancelled";
    }
    return "";
}

const char* JobPhaseLabel(JobPhase phase)
{
    switch (phase)
    {
    case JobPhase::Starting:
        return "starting";
    case JobPhase::Running:
        return "running";
    case JobPhase::Finished:
        return "finished";
    case JobPhase::FinishedWithFailures:
        return "finished with failures";
    case JobPhase::Cancelled:
        return "cancelled";
    case JobPhase::Refused:
        return "refused";
    case JobPhase::Busy:
        return "busy";
    case JobPhase::NoApiKey:
        return "no API key";
    case JobPhase::Failed:
        return "failed";
    }
    return "";
}

bool IsFinal(JobPhase phase)
{
    return phase != JobPhase::Starting && phase != JobPhase::Running;
}

JobItem& ConceptJobState::Item(const std::string& key)
{
    const auto it = std::find_if(items.begin(), items.end(), [&](const JobItem& item) { return item.key == key; });
    if (it != items.end())
        return *it;
    JobItem item;
    item.key = key;
    items.push_back(item);
    return items.back();
}

float ConceptJobState::Progress() const
{
    if (items.empty())
        return IsFinal(phase) ? 1.0f : 0.0f;
    const auto ended = std::count_if(items.begin(), items.end(), [](const JobItem& item) {
        return item.state == JobItemState::Done || item.state == JobItemState::Failed ||
               item.state == JobItemState::Cancelled;
    });
    return static_cast<float>(ended) / static_cast<float>(items.size());
}

bool ConceptJobState::CanResume() const
{
    return !resume.empty() && (phase == JobPhase::Cancelled || phase == JobPhase::FinishedWithFailures);
}

void ApplyEvent(ConceptJobState& state, const json& event, double now)
{
    const std::string name = Text(event, "event");
    if (name == "started")
        OnStarted(state, event);
    else if (name == "request_started")
        OnRequestStarted(state, event);
    else if (name == "retry")
        OnRetry(state, event, now);
    else if (name == "request_done")
        OnRequestDone(state, event);
    else if (name == "request_failed")
        OnRequestFailed(state, event);
    else if (name == "request_cancelled")
        OnRequestCancelled(state, event);
    else if (name == "finished" || name == "dry_run")
        OnFinished(state, event);
    else if (name == "cancelled")
        OnCancelled(state, event);
    else if (name == "refused")
        OnRefused(state, event);
    else if (name == "error")
        OnError(state, event);
}

bool ApplyLine(ConceptJobState& state, std::string_view line, double now)
{
    const json event = json::parse(line, nullptr, false);
    if (event.is_discarded() || !event.is_object() || Int(event, "protocol", 0) != PROTOCOL_VERSION ||
        !event.contains("event"))
        return false;
    ApplyEvent(state, event, now);
    return true;
}

void ApplyExit(ConceptJobState& state, int exitCode)
{
    state.exitCode = exitCode;
    if (IsFinal(state.phase))
        return;
    state.phase = PhaseOfExit(exitCode);
    if (state.phase == JobPhase::Finished)
        state.phase = JobPhase::Failed; // exit 0 without a final event: the output was cut short
    if (state.message.empty())
        state.message = ExitMessage(exitCode);
}

double RetrySecondsLeft(const JobItem& item, double now)
{
    if (item.state != JobItemState::Retrying)
        return 0.0;
    return std::max(0.0, item.retryDelay - (now - item.retrySince));
}
} // namespace Editor::Concepts

#endif // _EDITOR
