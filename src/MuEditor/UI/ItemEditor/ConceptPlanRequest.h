#pragma once

#ifdef _EDITOR

#include "Assets/ConceptPlan.h"
#include "Core/PythonTool.h"

#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <vector>

// The cost estimate a concepts dialog shows: `concepts.py plan ... --json` run in
// the background whenever the dialog's arguments change (after a short pause, so
// typing a note does not start a process per key), nothing spent. The plan shown
// is always the one for the current arguments, or none while it is being made.
class CConceptPlanRequest
{
public:
    // The arguments the dialog wants a plan for (PlanArgs / RefinePlanArgs).
    void Want(const std::vector<std::string>& arguments);
    // Runs the plan when the arguments settled; takes a finished result. Call once per frame.
    void Poll();
    // Forgets the plan (the dialog closed, or the references were rendered and it must be made again).
    void Reset();
    // Makes the plan again now (after a job changed what it depends on).
    void Refresh();

    // The plan for the current arguments, when it is ready.
    const Editor::Concepts::ConceptPlan* Current() const;
    bool IsWorking() const;
    // Why there is no plan (python3 missing, concepts.py wrote nothing).
    const std::string& Problem() const { return m_problem; }

private:
    void Launch();

    std::vector<std::string> m_wanted;
    std::vector<std::string> m_running; // the arguments of the plan being made
    std::vector<std::string> m_planned; // the arguments m_plan belongs to
    std::chrono::steady_clock::time_point m_changedAt;
    std::future<Editor::PythonTool::Result> m_future;
    std::optional<Editor::Concepts::ConceptPlan> m_plan;
    std::string m_problem;
};

#endif // _EDITOR
