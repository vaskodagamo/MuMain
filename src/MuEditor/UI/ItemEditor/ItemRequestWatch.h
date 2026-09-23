#pragma once

#ifdef _EDITOR

#include "Assets/ItemRequestScan.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

// The item requests on disk (assets-work/Items/requests/) as the Item Editor shows
// them: read once, then again whenever a folder, its request.json,
// owner-decision.json or delivery/ changes (checked every CHECK_INTERVAL by
// modification times, without reading a file) or Refresh() is pressed.
class CItemRequestWatch
{
public:
    static CItemRequestWatch& GetInstance();

    // Call once per frame while the Item Editor is shown.
    void Update();
    // Reads every request folder again now.
    void Refresh();

    // Empty without a repository checkout.
    const std::filesystem::path& RequestsDir() const { return m_requestsDir; }
    const std::vector<Editor::Assets::ItemRequestSummary>& Requests() const { return m_requests; }
    const Editor::Assets::ItemRequestSummary* Find(const std::string& id) const;
    // The requests per target item key (for Browse's status).
    const std::map<std::string, std::vector<Editor::Assets::RequestRef>>& ByItem() const { return m_byItem; }
    // The open, claimed or delivered requests naming `key`.
    std::vector<const Editor::Assets::ItemRequestSummary*> LiveRequestsFor(const std::string& key) const;
    // Changes whenever Requests() changed.
    std::uint64_t Version() const { return m_version; }

private:
    CItemRequestWatch() = default;
    void Scan();

    bool m_started = false;
    std::filesystem::path m_requestsDir;
    std::string m_fingerprint;
    std::chrono::steady_clock::time_point m_lastCheck;
    std::vector<Editor::Assets::ItemRequestSummary> m_requests;
    std::map<std::string, std::vector<Editor::Assets::RequestRef>> m_byItem;
    std::uint64_t m_version = 0;
};

#define g_ItemRequestWatch CItemRequestWatch::GetInstance()

#endif // _EDITOR
