#pragma once

#ifdef _EDITOR

#include "ItemRequestScan.h" // OwnerDecision

#include <filesystem>
#include <string>

// The owner's steps on a filed item request (assets-work/Items/requests/README.md,
// "Lifecycle"). The editor writes only what the README gives the owner:
// - Accept / Reject with notes on a delivered request: owner-decision.json next to
//   request.json, nothing else (the coordinator then records the decision);
// - Withdraw an open or claimed request: in request.json only status, decision and
//   one appended status_history entry; every frozen field stays as filed.
namespace Editor::Assets
{
constexpr const char* OWNER_VERDICT_ACCEPT = "accept";
constexpr const char* OWNER_VERDICT_REJECT = "reject";

// Writes `decision` as the request folder's owner-decision.json (replacing an
// earlier one). Refuses when the folder's request.json is not delivered.
bool WriteOwnerDecision(const std::filesystem::path& requestFolder, const OwnerDecision& decision,
                        std::string& error);

// Open and claimed requests may be withdrawn; decided ones may not.
bool IsWithdrawable(const std::string& status);

// Withdraws the request in `requestFolder` at `timestamp` (RFC 3339) with the
// owner's `reason` (may be empty). Refuses unless its status is open or claimed.
bool WithdrawRequest(const std::filesystem::path& requestFolder, const std::string& timestamp,
                     const std::string& reason, std::string& error);

// request.json text after the withdrawal (the same text otherwise, two-space
// indent); false with `error` when the text is not a request that may be withdrawn.
bool WithdrawnRequestText(const std::string& requestText, const std::string& timestamp, const std::string& reason,
                          std::string& out, std::string& error);
} // namespace Editor::Assets

#endif // _EDITOR
