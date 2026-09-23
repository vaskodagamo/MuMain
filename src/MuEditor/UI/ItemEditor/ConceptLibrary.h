#pragma once

#ifdef _EDITOR

#include "Assets/ConceptListing.h"
#include "Core/PythonTool.h"

#include <cstdint>
#include <future>
#include <string>

// Which items have concept images, for Browse's concept column: `concepts.py list
// --json` read in the background when the Item Editor opens, after every concepts
// job and after a pick, discard or unpick.
class CConceptLibrary
{
public:
    static CConceptLibrary& GetInstance();

    // Call once per frame while the Item Editor is shown.
    void Update();
    // Reads the list again (in the background).
    void Refresh();

    // The item's line, or null when it has no concept yet.
    const Editor::Concepts::ConceptSummary* Find(const std::string& itemKey) const;
    // Changes whenever the summaries were read again.
    std::uint64_t Version() const { return m_version; }
    const std::string& Problem() const { return m_problem; }

private:
    CConceptLibrary() = default;

    bool m_started = false;
    bool m_refreshWanted = false;
    std::uint64_t m_jobsSeen = 0;
    std::future<Editor::PythonTool::Result> m_future;
    Editor::Concepts::ConceptSummaries m_summaries;
    std::string m_problem;
    std::uint64_t m_version = 0;
};

#define g_ConceptLibrary CConceptLibrary::GetInstance()

#endif // _EDITOR
