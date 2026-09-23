#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptPlanRequest.h"

#include "ConceptTool.h"

namespace
{
// How long the arguments must stay the same before a plan runs.
constexpr std::chrono::milliseconds SETTLE_TIME(400);
} // namespace

void CConceptPlanRequest::Want(const std::vector<std::string>& arguments)
{
    if (arguments == m_wanted)
        return;
    m_wanted = arguments;
    m_changedAt = std::chrono::steady_clock::now();
}

void CConceptPlanRequest::Poll()
{
    namespace Tool = Editor::ItemEditor::ConceptTool;
    if (m_future.valid())
    {
        if (!Tool::IsReady(m_future))
            return;
        const Tool::JsonReply reply = Tool::ReplyOf(m_future.get());
        m_plan.reset();
        m_problem = reply.problem;
        Editor::Concepts::ConceptPlan plan;
        if (reply.ok && Editor::Concepts::ParseConceptPlan(reply.text, plan))
            m_plan = std::move(plan);
        else if (reply.ok)
            m_problem = "concepts.py answered something that is not a plan; see the log.";
        m_planned = m_running;
    }
    const bool settled = std::chrono::steady_clock::now() - m_changedAt >= SETTLE_TIME;
    if (!m_wanted.empty() && m_wanted != m_planned && settled)
        Launch();
}

void CConceptPlanRequest::Launch()
{
    m_running = m_wanted;
    m_future = Editor::ItemEditor::ConceptTool::RunJson(m_running);
}

void CConceptPlanRequest::Reset()
{
    m_wanted.clear();
    m_planned.clear();
    m_plan.reset();
    m_problem.clear();
}

void CConceptPlanRequest::Refresh()
{
    m_planned.clear();
    m_changedAt = {};
}

const Editor::Concepts::ConceptPlan* CConceptPlanRequest::Current() const
{
    return m_plan && m_planned == m_wanted && !m_wanted.empty() ? &*m_plan : nullptr;
}

bool CConceptPlanRequest::IsWorking() const
{
    return m_future.valid() || (m_wanted != m_planned && !m_wanted.empty());
}

#endif // _EDITOR
