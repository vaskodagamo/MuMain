#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptLibrary.h"

#include "ConceptJob.h"
#include "ConceptTool.h"

#include "Assets/ConceptCommands.h"

namespace Tool = Editor::ItemEditor::ConceptTool;

CConceptLibrary& CConceptLibrary::GetInstance()
{
    static CConceptLibrary instance;
    return instance;
}

void CConceptLibrary::Update()
{
    if (Tool::Repo().empty())
        return;
    if (!m_started || m_jobsSeen != g_ConceptJob.EndedVersion())
    {
        m_started = true;
        m_jobsSeen = g_ConceptJob.EndedVersion();
        m_refreshWanted = true;
    }
    if (Tool::IsReady(m_future))
    {
        const Tool::JsonReply reply = Tool::ReplyOf(m_future.get());
        Editor::Concepts::ConceptSummaries summaries;
        m_problem = reply.problem;
        if (reply.ok && Editor::Concepts::ParseConceptSummaries(reply.text, summaries) && summaries.ok)
            m_summaries = std::move(summaries);
        else if (reply.ok)
            m_problem = summaries.error.empty() ? "concepts.py list: unreadable answer" : summaries.error;
        ++m_version;
    }
    if (m_refreshWanted && !m_future.valid())
    {
        m_refreshWanted = false;
        m_future = Tool::RunJson(Editor::Concepts::ListAllArgs(Tool::Repo()));
    }
}

void CConceptLibrary::Refresh()
{
    m_refreshWanted = true;
}

const Editor::Concepts::ConceptSummary* CConceptLibrary::Find(const std::string& itemKey) const
{
    const auto it = m_summaries.byItemKey.find(itemKey);
    return it != m_summaries.byItemKey.end() ? &it->second : nullptr;
}

#endif // _EDITOR
