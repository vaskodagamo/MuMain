#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptPlanView.h"

#include "ConceptTool.h"

#include "Assets/EditorText.h"
#include "UI/MapEditor/MapEditorStatusLine.h"

#include "imgui.h"

using Editor::Concepts::ConceptPlan;
using Editor::Concepts::FormatDollars;

namespace Editor::ItemEditor::ConceptPlanView
{
namespace
{
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 OK_COLOR{0.45f, 0.85f, 0.45f, 1.0f};
constexpr ImVec4 ERROR_COLOR{1.0f, 0.45f, 0.4f, 1.0f};
constexpr const char* README = "assets-work/Items/concepts/README.md";

const char* SourceLabel(const std::string& source)
{
    if (source == "keychain")
        return "macOS Keychain";
    if (source == "env")
        return "OPENAI_API_KEY";
    return "unknown source";
}
} // namespace

std::string CostSplit(const Editor::Concepts::Cost& cost)
{
    return "text " + FormatDollars(cost.text) + " + reference " + FormatDollars(cost.reference) + " + output " +
           FormatDollars(cost.output) + " = " + FormatDollars(cost.total);
}

void RenderTotals(const ConceptPlan& plan)
{
    ImGui::Text("Estimate: %d images in %d requests (%s, %s, %s)", plan.images, plan.requests, plan.model.c_str(),
                plan.quality.c_str(), plan.size.c_str());
    ImGui::TextUnformatted(CostSplit(plan.total).c_str());
    for (const std::string& flag : plan.flags)
        ImGui::TextColored(NOTE_COLOR, "note: %s", flag.c_str());
    const bool within = plan.images <= plan.maxImages && plan.total.total <= plan.maxCost;
    ImGui::TextColored(within ? NOTE_COLOR : ERROR_COLOR, "Caps: at most %d images and %s per run - %s",
                       plan.maxImages, FormatDollars(plan.maxCost).c_str(), within ? "within both" : "above a cap");
}

void RenderApiKey(const ConceptPlan& plan)
{
    if (plan.apiKey.found)
    {
        ImGui::TextColored(OK_COLOR, "API key: found (%s)", SourceLabel(plan.apiKey.source));
        return;
    }
    ImGui::TextColored(ERROR_COLOR, "API key: not found.");
    ImGui::TextWrapped("Store it once in the macOS Keychain (service openai-api-key) as %s describes; the editor "
                       "never asks for it or shows it.",
                       README);
}

void RenderRefusal(const ConceptPlan& plan)
{
    if (!plan.wouldRefuse)
        return;
    ImGui::TextColored(ERROR_COLOR, "concepts.py would refuse this run:");
    for (const Editor::Concepts::PlanReason& reason : plan.reasons)
        ImGui::BulletText("%s", reason.message.c_str());
}

void RenderTestServerNote()
{
    const std::optional<std::string>& base = ConceptTool::TestApiBase();
    if (!base)
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
    ImGui::TextWrapped("Test mode: runs go to %s (MU_OPENAI_API_BASE), not to OpenAI.", base->c_str());
    ImGui::PopStyleColor();
}

void RenderPlanState(const ConceptPlan* plan, bool working, const std::string& problem)
{
    if (!problem.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", problem.c_str());
    else if (plan == nullptr && working)
        ImGui::TextColored(NOTE_COLOR, "Estimating (concepts.py plan, nothing is sent)...");
    else if (plan != nullptr && !plan->ok)
        ImGui::TextColored(ERROR_COLOR, "concepts.py plan: %s", plan->error.c_str());
}
} // namespace Editor::ItemEditor::ConceptPlanView

#endif // _EDITOR
