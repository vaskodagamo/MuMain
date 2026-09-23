#pragma once

#ifdef _EDITOR

#include "AssetCatalog.h" // RequestRef

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

// What the item request folders on disk say right now
// (<repo>/assets-work/Items/requests/<id>/): the Item Editor's Requests tab
// lists them and Browse takes "at Codex" / "delivered" from them, so both follow
// a filed, pulled or delivered request without a catalog rebuild. Files only.
namespace Editor::Assets
{
// requests/<id>/owner-decision.json, written by the editor's Accept / Reject.
struct OwnerDecision
{
    std::string verdict; // accept or reject
    std::string notes;
    std::string date; // YYYY-MM-DD
};

struct ItemRequestSummary
{
    std::string id; // the folder name
    std::filesystem::path folder;
    std::string problem; // why request.json could not be read; the fields below are then empty
    std::string status;  // open, claimed, delivered, accepted, rejected or withdrawn
    std::string kind;    // upscale, repaint, remodel, redesign or set
    std::string setKind; // set: the kind of every part
    std::string priority;
    std::string created;
    std::string branch;
    std::string claimedBy;
    std::optional<std::string> supersedes;
    std::string summary;
    std::vector<std::string> details;
    std::vector<std::string> keep;
    std::vector<std::string> avoid;
    std::vector<std::string> targetKeys; // the item the request was filed for first
    std::string decisionReason;          // decision.reason when decided
    bool deliveryPresent = false;        // the folder has delivery/
    std::optional<OwnerDecision> ownerDecision;
};

// Open, claimed or delivered: still waiting for the art builder or the owner.
bool IsLiveRequestStatus(const std::string& status);

// Fills `out` from request.json text (the id and folder are left as they are).
bool ParseItemRequestSummary(const std::string& text, ItemRequestSummary& out, std::string& error);

// Parses owner-decision.json text; false when it is not an object with a verdict.
bool ParseOwnerDecision(const std::string& text, OwnerDecision& out);

// Every folder under `requestsDir` that has a request.json, sorted by id. A
// folder whose request.json cannot be parsed is listed with `problem` set.
std::vector<ItemRequestSummary> ScanItemRequests(const std::filesystem::path& requestsDir);

// Changes whenever a request folder appears or goes away, or its request.json,
// owner-decision.json or delivery/ changes: folder names and modification times,
// read without opening a file.
std::string RequestsFingerprint(const std::filesystem::path& requestsDir);

// The scanned requests per target item key, as the catalog lists them.
std::map<std::string, std::vector<RequestRef>> RequestsByItem(const std::vector<ItemRequestSummary>& requests);
} // namespace Editor::Assets

#endif // _EDITOR
