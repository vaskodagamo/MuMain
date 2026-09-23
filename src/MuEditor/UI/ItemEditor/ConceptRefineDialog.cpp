#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptRefineDialog.h"

#include "ConceptImageCache.h"
#include "ConceptJob.h"
#include "ConceptPlanView.h"
#include "ConceptTool.h"

#include "Assets/EditorText.h"
#include "Core/MuEditorCore.h"
#include "Render/Renderer/MuRenderer.h"

#include "imgui.h"

using Editor::Concepts::ConceptPlan;
using Editor::Concepts::FormatDollars;
namespace Tool = Editor::ItemEditor::ConceptTool;
namespace View = Editor::ItemEditor::ConceptPlanView;

namespace
{
constexpr const char* POPUP_TITLE = "Refine concept";
constexpr float DIALOG_WIDTH = 640.0f;
constexpr float IMAGE_SIDE = 220.0f;
constexpr int IMAGE_PIXELS = 512;
constexpr float COMMENT_ROWS = 4.0f;
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 ERROR_COLOR{1.0f, 0.45f, 0.4f, 1.0f};

float Scaled(float pixels)
{
    return pixels * g_MuEditorCore.GetUIScale();
}
} // namespace

CConceptRefineDialog& CConceptRefineDialog::GetInstance()
{
    static CConceptRefineDialog instance;
    return instance;
}

void CConceptRefineDialog::Open(const Editor::Concepts::VariantRef& variant, const std::string& image,
                                const std::string& itemName)
{
    m_variant = variant;
    m_image = image;
    m_itemName = itemName;
    m_comment.fill('\0');
    m_error.clear();
    m_plan.Reset();
    m_open = true;
    m_openPending = true;
}

void CConceptRefineDialog::Render()
{
    if (!m_open)
        return;
    const std::string comment = Editor::Text::Trim(m_comment.data());
    if (comment.empty())
        m_plan.Reset();
    else
        m_plan.Want(Editor::Concepts::RefinePlanArgs(m_variant, comment, Tool::Repo()));
    m_plan.Poll();
    if (m_openPending)
    {
        ImGui::OpenPopup(POPUP_TITLE);
        m_openPending = false;
    }
    ImGui::SetNextWindowSize(ImVec2(Scaled(DIALOG_WIDTH), 0.0f), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal(POPUP_TITLE, nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        m_open = false;
        return;
    }
    g_MuEditorCore.SetHoveringUI(true);
    RenderVariant();
    ImGui::TextUnformatted("What should change? (the model revises this design, keeping the item's proportions)");
    ImGui::InputTextMultiline("##RefineComment", m_comment.data(), m_comment.size(),
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * COMMENT_ROWS));
    const ConceptPlan* plan = comment.empty() ? nullptr : m_plan.Current();
    ImGui::SeparatorText("Cost");
    if (comment.empty())
        ImGui::TextColored(NOTE_COLOR, "Write a comment to see the estimate.");
    else
        View::RenderPlanState(plan, m_plan.IsWorking(), m_plan.Problem());
    if (plan != nullptr && plan->ok)
    {
        View::RenderTotals(*plan);
        View::RenderApiKey(*plan);
        View::RenderRefusal(*plan);
    }
    View::RenderTestServerNote();
    ImGui::Separator();
    RenderButtons(plan);
    ImGui::EndPopup();
}

void CConceptRefineDialog::RenderVariant()
{
    const CConceptImageCache::Image& image = g_ConceptImages.Get(m_image, IMAGE_PIXELS);
    void* texture = image.texture != 0 ? mu::GetRenderer().GetTexturePointer(image.texture) : nullptr;
    const float side = Scaled(IMAGE_SIDE);
    if (texture != nullptr)
        ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2(side, side));
    else
        ImGui::Dummy(ImVec2(side, side));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("%s", m_itemName.c_str());
    ImGui::TextColored(NOTE_COLOR, "from %s", Editor::Concepts::VariantPath(m_variant).c_str());
    ImGui::TextWrapped("The new variants go into a new batch and remember this one as their parent.");
    ImGui::EndGroup();
}

void CConceptRefineDialog::RenderButtons(const ConceptPlan* plan)
{
    const bool busy = g_ConceptJob.IsRunning();
    const bool ready = plan != nullptr && plan->ok && !plan->wouldRefuse && !busy;
    const std::string label =
        plan != nullptr && plan->ok ? "Refine for " + FormatDollars(plan->total.total) : std::string("Refine");
    ImGui::BeginDisabled(!ready);
    if (ImGui::Button(label.c_str()))
        StartRun(*plan);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
        Close();
    if (busy)
        ImGui::TextColored(NOTE_COLOR, "A concepts job is running; wait for it or cancel it first.");
    if (!m_error.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", m_error.c_str());
}

void CConceptRefineDialog::StartRun(const ConceptPlan& plan)
{
    const std::string comment = Editor::Text::Trim(m_comment.data());
    const std::string title = "Refine " + m_itemName + " " + m_variant.variant + " (estimate " +
                              FormatDollars(plan.total.total) + ")";
    std::vector<CConceptJob::Item> items = {{m_variant.key, m_itemName}};
    std::vector<std::string> arguments =
        Editor::Concepts::RefineRunArgs(m_variant, comment, Tool::Repo(), Tool::TestApiBase());
    if (!g_ConceptJob.Start(CConceptJob::Kind::Refine, title, std::move(arguments), items, m_error))
        return;
    Close();
}

void CConceptRefineDialog::Close()
{
    m_open = false;
    m_plan.Reset();
    ImGui::CloseCurrentPopup();
}

#endif // _EDITOR
