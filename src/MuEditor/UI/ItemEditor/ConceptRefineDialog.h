#pragma once

#ifdef _EDITOR

#include "ConceptPlanRequest.h"

#include "Assets/ConceptCommands.h"

#include <array>
#include <string>

// "Refine with comment...": a new round from one concept variant. The owner writes
// what to change; the dialog shows the variant, what `concepts.py plan --from
// <variant> --note ...` estimates for it (its own cost confirmation) and starts
// `run --from ... --yes` as a background job. The new variants record the one they
// came from, so the Concepts panel shows their lineage.
class CConceptRefineDialog
{
public:
    static CConceptRefineDialog& GetInstance();

    // `image` is the variant's PNG, `itemName` what to call the item.
    void Open(const Editor::Concepts::VariantRef& variant, const std::string& image, const std::string& itemName);
    // Call once per frame inside the Item Editor window.
    void Render();

private:
    static constexpr std::size_t COMMENT_CHARS = 1024;

    CConceptRefineDialog() = default;
    void RenderVariant();
    void RenderButtons(const Editor::Concepts::ConceptPlan* plan);
    void StartRun(const Editor::Concepts::ConceptPlan& plan);
    void Close();

    bool m_open = false;
    bool m_openPending = false;
    Editor::Concepts::VariantRef m_variant;
    std::string m_image;
    std::string m_itemName;
    std::array<char, COMMENT_CHARS> m_comment = {};
    CConceptPlanRequest m_plan;
    std::string m_error;
};

#define g_ConceptRefineDialog CConceptRefineDialog::GetInstance()

#endif // _EDITOR
