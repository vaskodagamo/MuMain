#include "stdafx.h"

#ifdef _EDITOR

#include "ItemBrowseDetails.h"

#include "ItemThumbnailView.h"

#include "Assets/EditorText.h"
#include "Core/MuEditorCore.h"

#include "imgui.h"

#include <vector>

namespace Editor::ItemEditor
{
namespace
{
using Assets::ItemCatalogEntry;
using Assets::ItemModel;

constexpr float PREVIEW_SIZE = 224.0f; // twice the thumbnail; I4's 3D preview takes this place
constexpr float PREVIEW_PADDING = 6.0f;
constexpr ImU32 PREVIEW_BACKGROUND = IM_COL32(24, 24, 28, 255);
constexpr ImU32 PREVIEW_BORDER = IM_COL32(110, 110, 120, 255);
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 WARNING_COLOR{1.0f, 0.8f, 0.4f, 1.0f};
constexpr const char* NO_NAME = "(no name in the item table)";
constexpr const char* PLAYER_FOLDER = "/Data/Player/"; // armour parts load from the character models

// The thumbnail in a framed square, with a note that the live preview comes later.
void RenderPreviewArea(const Items::BrowseRow& row)
{
    const float size = PREVIEW_SIZE * g_MuEditorCore.GetUIScale();
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(corner, ImVec2(corner.x + size, corner.y + size), PREVIEW_BACKGROUND);
    drawList->AddRect(corner, ImVec2(corner.x + size, corner.y + size), PREVIEW_BORDER);
    ImGui::SetCursorScreenPos(ImVec2(corner.x + PREVIEW_PADDING, corner.y + PREVIEW_PADDING));
    DrawItemThumbnail(row, size - 2.0f * PREVIEW_PADDING);
    ImGui::SetCursorScreenPos(ImVec2(corner.x, corner.y + size));
    ImGui::TextColored(NOTE_COLOR, "Thumbnail - the 3D preview (level, excellent,");
    ImGui::TextColored(NOTE_COLOR, "ancient, equipped) comes here in I4.");
}

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
        ImGui::TextColored(NOTE_COLOR, "The thumbnail shows the first model (the male part); variants follow it.");
    if (!item.originalRevision.empty())
        ImGui::TextColored(NOTE_COLOR, "Original: commit %.10s", item.originalRevision.c_str());
}

void RenderRequests(const ItemCatalogEntry& item)
{
    if (item.requests.empty() && !item.clientReview)
        return;
    ImGui::SeparatorText("Requests and verdict");
    for (const Assets::RequestRef& request : item.requests)
    {
        const char* assigned = request.assignedTo.empty() ? "unassigned" : request.assignedTo.c_str();
        ImGui::BulletText("%s: %s (%s)", request.id.c_str(), request.status.c_str(), assigned);
    }
    if (item.clientReview)
        ImGui::BulletText("Owner: %s %s", item.clientReview->verdict.c_str(), item.clientReview->note.c_str());
}
} // namespace

void RenderItemDetails(const Items::BrowseRow& row, const std::string& catalogNote)
{
    ImGui::TextUnformatted(row.name.empty() ? NO_NAME : row.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(NOTE_COLOR, "%s (type %d)", row.key.c_str(), row.type);
    RenderPreviewArea(row);

    if (ImGui::BeginTable("ItemFacts", 2, ImGuiTableFlags_SizingStretchProp))
    {
        RenderTableFacts(row);
        if (row.catalog != nullptr)
            RenderCatalogFacts(*row.catalog);
        ImGui::EndTable();
    }
    if (row.catalog == nullptr)
    {
        ImGui::TextColored(WARNING_COLOR, "No catalog facts: %s", catalogNote.c_str());
        return;
    }
    RenderModels(*row.catalog);
    RenderRequests(*row.catalog);
}
} // namespace Editor::ItemEditor

#endif // _EDITOR
