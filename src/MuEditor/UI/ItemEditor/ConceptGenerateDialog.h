#pragma once

#ifdef _EDITOR

#include "ConceptPlanRequest.h"

#include "Assets/ConceptCommands.h"
#include "Assets/ConceptPlan.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// "Generate concepts (N)...": concept images for the items selected in Browse.
// The owner picks the preset (explore / final), the variants per item, the
// turnaround sheet and notes (one for all, one per item); the dialog shows what
// `concepts.py plan` estimates for exactly that - per item and in total, split
// into text, reference and output, the caps and any reason the tool would refuse,
// and whether an API key was found - and offers to render missing reference images
// first. "Generate for $X" starts the run as a background job (ConceptJob.h).
class CConceptGenerateDialog
{
public:
    struct Item
    {
        std::string key;
        std::string name;
    };

    static CConceptGenerateDialog& GetInstance();

    void Open(std::vector<Item> items);
    // Draws the dialog while it is open; call once per frame inside the Item Editor window.
    void Render();

private:
    static constexpr std::size_t NOTE_CHARS = 512;
    using NoteBuffer = std::array<char, NOTE_CHARS>;

    CConceptGenerateDialog() = default;
    void ReadPresets();
    void ChoosePreset(int index);
    std::vector<std::string> Keys() const;
    Editor::Concepts::GenerateSettings Settings() const;
    const Editor::Concepts::PlanItem* PlanItemOf(const Editor::Concepts::ConceptPlan* plan,
                                                 const std::string& key) const;

    void RenderSettings();
    void RenderItems(const Editor::Concepts::ConceptPlan* plan);
    void RenderItemRow(const Item& item, const Editor::Concepts::ConceptPlan* plan);
    void RenderReferences(const Editor::Concepts::ConceptPlan* plan);
    void RenderEstimate(const Editor::Concepts::ConceptPlan* plan);
    void RenderButtons(const Editor::Concepts::ConceptPlan* plan);
    void StartRun(const Editor::Concepts::ConceptPlan& plan);
    void Close();

    bool m_open = false;
    bool m_openPending = false;
    std::vector<Item> m_items;
    Editor::Concepts::ConceptPresets m_presets;
    int m_preset = 0;
    int m_variants = 0;
    bool m_sheet = false;
    NoteBuffer m_note = {};
    std::map<std::string, NoteBuffer> m_itemNotes;
    CConceptPlanRequest m_plan;
    std::uint64_t m_jobsSeen = 0;
    std::string m_error;
};

#define g_ConceptGenerateDialog CConceptGenerateDialog::GetInstance()

#endif // _EDITOR
