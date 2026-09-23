#include "ItemRequestDecision.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "JsonFields.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;
using nlohmann::ordered_json;

namespace Editor::Assets
{
namespace
{
constexpr const char* REQUEST_FILE = "request.json";
constexpr const char* OWNER_DECISION_FILE = "owner-decision.json";
constexpr const char* STATUS_DELIVERED = "delivered";
constexpr const char* STATUS_WITHDRAWN = "withdrawn";
constexpr const char* WITHDRAWABLE_STATES[] = {"open", "claimed"};
constexpr const char* WITHDRAWN_BY = "owner (item editor)";
constexpr int JSON_INDENT = 2;

bool ReadText(const fs::path& file, std::string& text, std::string& error)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
    {
        error = "cannot read " + Editor::Text::PathToUtf8(file);
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

std::string Dump(const ordered_json& document)
{
    // Invalid UTF-8 in typed notes becomes U+FFFD instead of failing the write.
    return document.dump(JSON_INDENT, ' ', false, ordered_json::error_handler_t::replace) + "\n";
}

std::string StatusOf(const ordered_json& request)
{
    const auto status = request.find("status");
    return status != request.end() && status->is_string() ? status->get<std::string>() : std::string();
}
} // namespace

bool IsWithdrawable(const std::string& status)
{
    return std::any_of(std::begin(WITHDRAWABLE_STATES), std::end(WITHDRAWABLE_STATES),
                       [&status](const char* state) { return status == state; });
}

bool WriteOwnerDecision(const fs::path& requestFolder, const OwnerDecision& decision, std::string& error)
{
    if (decision.verdict != OWNER_VERDICT_ACCEPT && decision.verdict != OWNER_VERDICT_REJECT)
    {
        error = "the verdict is accept or reject";
        return false;
    }
    std::string text;
    if (!ReadText(requestFolder / REQUEST_FILE, text, error))
        return false;
    const ordered_json request = ordered_json::parse(text, nullptr, false);
    if (request.is_discarded() || !request.is_object() || StatusOf(request) != STATUS_DELIVERED)
    {
        error = "only a delivered request can be accepted or rejected (pull the worker's branch first)";
        return false;
    }
    ordered_json document;
    document["verdict"] = decision.verdict;
    document["notes"] = decision.notes;
    document["date"] = decision.date;
    return Json::ReplaceFileText(requestFolder / OWNER_DECISION_FILE, Dump(document), error);
}

bool WithdrawnRequestText(const std::string& requestText, const std::string& timestamp, const std::string& reason,
                          std::string& out, std::string& error)
{
    ordered_json request = ordered_json::parse(requestText, nullptr, false);
    if (request.is_discarded() || !request.is_object())
    {
        error = "request.json is not a JSON object";
        return false;
    }
    const std::string status = StatusOf(request);
    if (!IsWithdrawable(status))
    {
        error = "a " + (status.empty() ? std::string("request without status") : status + " request") +
                " cannot be withdrawn (only open or claimed ones)";
        return false;
    }
    ordered_json& history = request["status_history"];
    if (!history.is_array())
        history = ordered_json::array();
    ordered_json entry = {{"status", STATUS_WITHDRAWN}, {"at", timestamp}, {"by", WITHDRAWN_BY}};
    if (!reason.empty())
        entry["note"] = reason;
    history.push_back(entry);
    request["status"] = STATUS_WITHDRAWN;
    request["decision"] = {{"status", STATUS_WITHDRAWN},
                           {"at", timestamp},
                           {"by", WITHDRAWN_BY},
                           {"reason", reason},
                           {"ledger_entry", nullptr}};
    out = Dump(request);
    return true;
}

bool WithdrawRequest(const fs::path& requestFolder, const std::string& timestamp, const std::string& reason,
                     std::string& error)
{
    const fs::path file = requestFolder / REQUEST_FILE;
    std::string text;
    std::string withdrawn;
    if (!ReadText(file, text, error) || !WithdrawnRequestText(text, timestamp, reason, withdrawn, error))
        return false;
    return Json::ReplaceFileText(file, withdrawn, error);
}
} // namespace Editor::Assets

#endif // _EDITOR
