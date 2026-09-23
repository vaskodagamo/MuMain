#include "stdafx.h"

#ifdef _EDITOR

#include "ItemRequestWatch.h"

#include "Assets/RegenRequest.h"
#include "Assets/RequestFolder.h"
#include "Core/EditorFiles.h"

#include <algorithm>

namespace
{
// How often the requests folder is checked for changes (a few stat calls).
constexpr std::chrono::milliseconds CHECK_INTERVAL(1500);
} // namespace

CItemRequestWatch& CItemRequestWatch::GetInstance()
{
    static CItemRequestWatch instance;
    return instance;
}

void CItemRequestWatch::Update()
{
    const auto now = std::chrono::steady_clock::now();
    if (!m_started)
    {
        m_started = true;
        const std::filesystem::path& repo = Editor::Files::RepoRoot().root;
        if (!repo.empty())
            m_requestsDir = Editor::Assets::RequestsDir(repo, Editor::Assets::ItemRequestDomain());
        m_lastCheck = now;
        Scan();
        return;
    }
    if (m_requestsDir.empty() || now - m_lastCheck < CHECK_INTERVAL)
        return;
    m_lastCheck = now;
    if (Editor::Assets::RequestsFingerprint(m_requestsDir) != m_fingerprint)
        Scan();
}

void CItemRequestWatch::Refresh()
{
    if (!m_started)
    {
        Update();
        return;
    }
    Scan();
}

void CItemRequestWatch::Scan()
{
    if (m_requestsDir.empty())
        return;
    m_fingerprint = Editor::Assets::RequestsFingerprint(m_requestsDir);
    m_requests = Editor::Assets::ScanItemRequests(m_requestsDir);
    m_byItem = Editor::Assets::RequestsByItem(m_requests);
    ++m_version;
}

const Editor::Assets::ItemRequestSummary* CItemRequestWatch::Find(const std::string& id) const
{
    const auto it = std::find_if(m_requests.begin(), m_requests.end(),
                                 [&id](const Editor::Assets::ItemRequestSummary& request) { return request.id == id; });
    return it != m_requests.end() ? &*it : nullptr;
}

std::vector<const Editor::Assets::ItemRequestSummary*> CItemRequestWatch::LiveRequestsFor(const std::string& key) const
{
    std::vector<const Editor::Assets::ItemRequestSummary*> live;
    for (const Editor::Assets::ItemRequestSummary& request : m_requests)
    {
        const bool names = std::find(request.targetKeys.begin(), request.targetKeys.end(), key) !=
                           request.targetKeys.end();
        if (names && Editor::Assets::IsLiveRequestStatus(request.status))
            live.push_back(&request);
    }
    return live;
}

#endif // _EDITOR
