#include "stdafx.h"

#ifdef _EDITOR

#include "ItemBrowseDetails.h"

#include "ConceptsPanel.h"
#include "ItemAbCompare.h"
#include "ItemOwnerActions.h"
#include "ItemPreview.h"

#include "Assets/EditorText.h"

#include "imgui.h"

#include <vector>

namespace Editor::ItemEditor
{
namespace
{
using Assets::ItemCatalogEntry;
using Assets::ItemModel;

constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 WARNING_COLOR{1.0f, 0.8f, 0.4f, 1.0f};
constexpr const char* NO_NAME = "(no name in the item table)";
constexpr const char* PLAYER_FOLDER = "/Data/Player/"; // armour parts load from the character models

std::string BadgeText(const Assets::ItemBadges& badges)
{
    std::vector<std::string> parts;
    if (badges.socket)
        parts.emplace_back("socket");
    if (badges.set)
        parts.emplace_back("ancient set");
    if (badges.option380)
        parts.emplace_back("380 option");
    if (badges.excellent)
        parts.emplace_back("excellent");
    if (badges.maxItemLevel > 0)
        parts.push_back("up to +" + std::to_string(badges.maxItemLevel));
    return Editor::Text::Join(parts, ", ");
}

void RenderFact(const char* label, const std::string& value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", value.c_str());
}

std::string TierText(const ItemCatalogEntry& item)
{
    const Assets::ItemTier& tier = item.tier;
    std::string text = "T" + std::to_string(tier.value) + " - " + std::to_string(tier.familyRank) + " of " +
                       std::to_string(tier.familySize) + " in " + item.family + " (basic -> rare)";
    if (!tier.dropsFromMonsters)
        text += ", never drops from monsters";
    if (tier.source == "owner")
        text += ", set by the owner";
    return text;
}

void RenderTableFacts(const Items::BrowseRow& row)
{
    const std::string classes = Items::ClassSummary(row.requireClass);
    RenderFact("Classes", classes.empty() ? "none" : classes + " (class and lowest stage)");
    RenderFact("Required level", std::to_string(row.requireLevel));
    RenderFact("Drop level", std::to_string(row.dropLevel));
    RenderFact("Status", Items::StatusLabel(row.status));
}

void RenderCatalogFacts(const ItemCatalogEntry& item)
{
    RenderFact("Family", item.family);
    RenderFact("Tier", TierText(item));
    const std::string badges = BadgeText(item.badges);
    RenderFact("Can be", badges.empty() ? "-" : badges);
    std::string size = std::to_string(item.width) + " x " + std::to_string(item.height) + " slots";
    if (item.twoHand)
        size += ", two-handed";
    RenderFact("Size", size);
}

void RenderDrawnFact(const Items::BrowseRow& row, const Assets::ItemRenderFacts* renderFacts)
{
    const Assets::ItemRenderEntry* render = renderFacts != nullptr ? renderFacts->Find(row.key) : nullptr;
    if (render == nullptr)
    {
        RenderFact("Drawn", "unknown (build assets-work/Items/render-facts.json)");
        return;
    }
    RenderFact("Drawn", render->drawn);
    if (!render->summary.empty() && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", render->summary.c_str());
}

void RenderModel(const ItemModel& model)
{
    std::string heading = model.role;
    if (!model.className.empty())
        heading += " (" + model.className + ")";
    ImGui::BulletText("%s: %s", heading.c_str(), model.bmd.c_str());
    ImGui::Indent();
    ImGui::Text("%d triangles, %d meshes%s", model.triangles, model.meshes, model.exists ? "" : " - FILE MISSING");
    if (!model.condition.empty())
        ImGui::TextColored(NOTE_COLOR, "drawn when: %s", model.condition.c_str());
    for (const Assets::ItemTexture& texture : model.textures)
    {
        const char* container = texture.container.empty() ? "(hidden or missing)" : texture.container.c_str();
        ImGui::TextColored(NOTE_COLOR, "%s -> %s", texture.name.c_str(), container);
    }
    ImGui::Unindent();
}

void RenderModels(const ItemCatalogEntry& item)
{
    ImGui::SeparatorText("Model files");
    if (item.models.empty())
        ImGui::TextColored(NOTE_COLOR, "The catalog lists no model file.");
    for (const ItemModel& model : item.models)
        RenderModel(model);
    if (!item.models.empty() && item.models.front().bmd.find(PLAYER_FOLDER) != std::string::npos)
        ImGui::TextColored(NOTE_COLOR, "Thumbnails show the first model (the male part); variants follow it.");
    if (!item.originalRevision.empty())
        ImGui::TextColored(NOTE_COLOR, "Original: commit %.10s", item.originalRevision.c_str());
}

} // namespace

void RenderItemDetails(const Items::BrowseRow& row, const std::string& catalogNote, int filterClass, int filterStage,
                       const Assets::ItemCatalog* catalog, const Assets::ItemRenderFacts* renderFacts)
{
    ImGui::TextUnformatted(row.name.empty() ? NO_NAME : row.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(NOTE_COLOR, "%s (type %d)", row.key.c_str(), row.type);
    if (row.catalog != nullptr)
        g_ItemAbCompare.PassCompareToPreview(*row.catalog);
    g_ItemPreview.Render(row, filterClass, filterStage);
    if (row.catalog != nullptr && catalog != nullptr)
    {
        g_ItemAbCompare.Render(*row.catalog, *catalog);
        RenderOwnerActions(*row.catalog, *catalog);
        g_ConceptsPanel.Render(*row.catalog, *catalog);
    }

    if (ImGui::BeginTable("ItemFacts", 2, ImGuiTableFlags_SizingStretchProp))
    {
        RenderTableFacts(row);
        if (row.catalog != nullptr)
            RenderCatalogFacts(*row.catalog);
        RenderDrawnFact(row, renderFacts);
        ImGui::EndTable();
    }
    if (row.catalog == nullptr)
    {
        ImGui::TextColored(WARNING_COLOR, "No catalog facts: %s", catalogNote.c_str());
        return;
    }
    RenderModels(*row.catalog);
}
} // namespace Editor::ItemEditor

#endif // _EDITOR
