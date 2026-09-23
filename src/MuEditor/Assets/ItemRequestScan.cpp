#include "ItemRequestScan.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "JsonFields.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Editor::Assets
{
namespace
{
using Json::Member;
using Json::OptionalText;
using Json::Text;
using Json::TextList;

constexpr const char* REQUEST_FILE = "request.json";
constexpr const char* OWNER_DECISION_FILE = "owner-decision.json";
constexpr const char* DELIVERY_FOLDER = "delivery";
constexpr const char* LIVE_STATES[] = {"open", "claimed", "delivered"};

std::string ReadText(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

std::vector<std::string> TargetKeys(const json& request)
{
    std::vector<std::string> keys;
    for (const json& target : Json::Array(request, "targets"))
    {
        const std::string key = Text(target, "key");
        if (!key.empty())
            keys.push_back(key);
    }
    return keys;
}

// Modification time of `path` as a number, or -1 when it does not exist.
long long ModifiedTicks(const fs::path& path)
{
    std::error_code ec;
    const fs::file_time_type time = fs::last_write_time(path, ec);
    return ec ? -1 : static_cast<long long>(time.time_since_epoch().count());
}

ItemRequestSummary ReadSummary(const fs::path& folder)
{
    ItemRequestSummary summary;
    summary.id = Editor::Text::PathToUtf8(folder.filename());
    summary.folder = folder;
    std::string error;
    if (!ParseItemRequestSummary(ReadText(folder / REQUEST_FILE), summary, error))
        summary.problem = error;
    std::error_code ec;
    summary.deliveryPresent = fs::is_directory(folder / DELIVERY_FOLDER, ec);
    const fs::path decisionFile = folder / OWNER_DECISION_FILE;
    OwnerDecision decision;
    if (fs::exists(decisionFile, ec) && ParseOwnerDecision(ReadText(decisionFile), decision))
        summary.ownerDecision = decision;
    return summary;
}
} // namespace

bool IsLiveRequestStatus(const std::string& status)
{
    return std::any_of(std::begin(LIVE_STATES), std::end(LIVE_STATES),
                       [&status](const char* state) { return status == state; });
}

bool ParseItemRequestSummary(const std::string& text, ItemRequestSummary& out, std::string& error)
{
    const json request = json::parse(text, nullptr, false);
    if (request.is_discarded() || !request.is_object())
    {
        error = "request.json is not a JSON object";
        return false;
    }
    out.status = Text(request, "status");
    out.kind = Text(request, "kind");
    out.priority = Text(request, "priority");
    out.created = Text(request, "created");
    out.supersedes = OptionalText(request, "supersedes");
    const json& change = Member(request, "change");
    out.setKind = Text(change, "set_kind");
    out.summary = Text(change, "summary");
    out.details = TextList(change, "details");
    out.keep = TextList(change, "keep");
    out.avoid = TextList(change, "avoid");
    const json& handoff = Member(request, "handoff");
    out.branch = Text(handoff, "branch");
    out.claimedBy = Text(handoff, "claimed_by");
    out.decisionReason = Text(Member(request, "decision"), "reason");
    out.targetKeys = TargetKeys(request);
    return true;
}

bool ParseOwnerDecision(const std::string& text, OwnerDecision& out)
{
    const json decision = json::parse(text, nullptr, false);
    if (decision.is_discarded() || !decision.is_object())
        return false;
    out = OwnerDecision{Text(decision, "verdict"), Text(decision, "notes"), Text(decision, "date")};
    return !out.verdict.empty();
}

std::vector<ItemRequestSummary> ScanItemRequests(const fs::path& requestsDir)
{
    std::vector<ItemRequestSummary> requests;
    std::error_code ec;
    for (fs::directory_iterator it(requestsDir, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code fileError;
        if (it->is_directory(fileError) && fs::exists(it->path() / REQUEST_FILE, fileError))
            requests.push_back(ReadSummary(it->path()));
    }
    std::sort(requests.begin(), requests.end(),
              [](const ItemRequestSummary& a, const ItemRequestSummary& b) { return a.id < b.id; });
    return requests;
}

std::string RequestsFingerprint(const fs::path& requestsDir)
{
    std::vector<std::string> parts;
    std::error_code ec;
    for (fs::directory_iterator it(requestsDir, ec), end; !ec && it != end; it.increment(ec))
    {
        const fs::path& folder = it->path();
        std::error_code fileError;
        if (!it->is_directory(fileError))
            continue;
        parts.push_back(Editor::Text::PathToUtf8(folder.filename()) + ":" +
                        std::to_string(ModifiedTicks(folder / REQUEST_FILE)) + ":" +
                        std::to_string(ModifiedTicks(folder / OWNER_DECISION_FILE)) + ":" +
                        std::to_string(ModifiedTicks(folder / DELIVERY_FOLDER)));
    }
    std::sort(parts.begin(), parts.end());
    return Editor::Text::Join(parts, "|");
}

std::map<std::string, std::vector<RequestRef>> RequestsByItem(const std::vector<ItemRequestSummary>& requests)
{
    std::map<std::string, std::vector<RequestRef>> byItem;
    for (const ItemRequestSummary& request : requests)
    {
        for (const std::string& key : request.targetKeys)
            byItem[key].push_back({request.id, request.status, request.claimedBy});
    }
    return byItem;
}
} // namespace Editor::Assets

#endif // _EDITOR
