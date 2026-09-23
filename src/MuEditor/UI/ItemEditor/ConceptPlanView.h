#pragma once

#ifdef _EDITOR

#include "Assets/ConceptPlan.h"

#include <string>

// The parts of a concepts cost estimate both concepts dialogs (Generate, Refine)
// show: the cost split and totals, the caps, whether an API key was found (only
// that and where, never the key), why `run` would refuse, and the test-server note.
namespace Editor::ItemEditor::ConceptPlanView
{
// "text $0.02 + reference $0.07 + output $0.12 = $0.21".
std::string CostSplit(const Editor::Concepts::Cost& cost);

// The totals, flags and caps.
void RenderTotals(const Editor::Concepts::ConceptPlan& plan);
// "API key: found (macOS Keychain)" or where to put one.
void RenderApiKey(const Editor::Concepts::ConceptPlan& plan);
// The reasons `run` would refuse; nothing when it would not.
void RenderRefusal(const Editor::Concepts::ConceptPlan& plan);
// While MU_OPENAI_API_BASE points the runs at a local test server.
void RenderTestServerNote();
// A plan that failed ("ok": false) or is still being made.
void RenderPlanState(const Editor::Concepts::ConceptPlan* plan, bool working, const std::string& problem);
} // namespace Editor::ItemEditor::ConceptPlanView

#endif // _EDITOR
