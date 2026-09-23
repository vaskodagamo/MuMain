#include "stdafx.h"

#ifdef _EDITOR

#include "MapAssetVariants.h"

#include "MapEditorFileUtil.h"
#include "MapEditorStatusLine.h"

#include "Core/ModelHotReload.h"

#include "imgui.h"

#include <optional>
#include <set>
#include <vector>

using namespace Editor::Assets;
namespace HotReload = Editor::Assets::HotReload;

namespace
{
// A map load replaces every model; notice it within about a second.
constexpr int REFRESH_INTERVAL_FRAMES = 60;

const ImVec4 COLOR_ORIGINAL(0.55f, 0.8f, 1.0f, 1.0f);

bool IsBusy()
{
    return HotReload::PendingCount() > 0;
}

void HoverTip(const char* text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", text);
}
} // namespace

CMapAssetVariants& CMapAssetVariants::GetInstance()
{
    static CMapAssetVariants instance;
    return instance;
}

void CMapAssetVariants::EnsureWorld(int world)
{
    if (world == m_world)
        return;
    m_world = world;
    m_repo = Editor::Files::RepoRoot().root;
    m_originalRoot = VariantDataRoot(m_repo, AssetVariant::Original);
    m_command = MaterializeCommand(m_repo, AssetVariant::Original, world);
    m_shown.clear();
    m_refreshFrame = -1;
    m_status.clear();
    CheckOriginalFiles();
}

void CMapAssetVariants::Reset()
{
    m_world = -1;
}

void CMapAssetVariants::CheckOriginalFiles()
{
    m_originalState = m_repo.empty() ? VariantState::Missing : CheckVariant(m_repo, AssetVariant::Original, m_world);
}

void CMapAssetVariants::TakeOutcomes()
{
    const std::vector<HotReload::Outcome> outcomes = HotReload::TakeOutcomes(HotReload::WorldObjectRange());
    if (outcomes.empty())
        return;
    for (const HotReload::Outcome& outcome : outcomes)
    {
        ++m_batchDone;
        if (!outcome.loaded && m_batchRefused++ == 0)
            m_firstRefusal = outcome.message;
        m_status = outcome.message;
    }
    m_refreshFrame = -1;
    if (m_batchTotal <= 1 || IsBusy())
        return;
    m_status = "All models: " + std::to_string(m_batchDone - m_batchRefused) + " now show their " +
               VariantName(m_batchVariant) + " files.";
    if (m_batchRefused > 0)
        m_status +=
            " " + std::to_string(m_batchRefused) + " not changed (see the Editor Console), first: " + m_firstRefusal;
}

void CMapAssetVariants::RefreshShown(const Catalog& catalog)
{
    const int frame = ImGui::GetFrameCount();
    if (m_refreshFrame >= 0 && frame - m_refreshFrame < REFRESH_INTERVAL_FRAMES)
        return;
    m_refreshFrame = frame;
    for (const CatalogModel& model : catalog.models)
    {
        if (model.type != NO_TYPE)
            m_shown[model.type] = ShownFor(model.type);
    }
}

CMapAssetVariants::Shown CMapAssetVariants::ShownFor(int type) const
{
    if (!HotReload::CanReload(type))
        return Shown::NotLoaded;
    const std::optional<AssetVariant> model = HotReload::LoadedVariant(type);
    const HotReload::TextureOrigins textures = HotReload::CountTexturesFrom(type, m_originalRoot);
    if (!model)
        return textures.fromFolder == 0 ? Shown::AsBuilt : Shown::Mixed;
    if (*model == AssetVariant::Original && textures.fromFolder == textures.total)
        return Shown::Original;
    if (*model == AssetVariant::Current && textures.fromFolder == 0)
        return Shown::Current;
    return Shown::Mixed;
}

CMapAssetVariants::Shown CMapAssetVariants::ShownOf(const CatalogModel& model) const
{
    const auto it = m_shown.find(model.type);
    return it != m_shown.end() ? it->second : Shown::NotLoaded;
}

const char* CMapAssetVariants::ShownLabel(const CatalogModel& model) const
{
    switch (ShownOf(model))
    {
    case Shown::AsBuilt:
        return "as built";
    case Shown::Current:
        return "current";
    case Shown::Original:
        return "original";
    case Shown::Mixed:
        return "mixed";
    default:
        return "-";
    }
}

bool CMapAssetVariants::QueueReload(const CatalogModel& model, AssetVariant variant)
{
    const std::optional<std::filesystem::path> file = VariantFile(m_repo, variant, model.bmd);
    if (!file)
    {
        m_status = model.name + ": its model file " + model.bmd + " is not under src/bin/Data.";
        return false;
    }
    HotReload::Queue({model.type, model.name, variant, *file});
    return true;
}

void CMapAssetVariants::QueueOne(const CatalogModel& model, AssetVariant variant)
{
    m_batchVariant = variant;
    m_batchTotal = QueueReload(model, variant) ? 1 : 0;
    m_batchDone = 0;
    m_batchRefused = 0;
    if (m_batchTotal == 1)
        m_status = "Loading " + model.name + " (" + VariantName(variant) + ")...";
}

void CMapAssetVariants::QueueAll(const Catalog& catalog, AssetVariant variant)
{
    m_batchVariant = variant;
    m_batchTotal = 0;
    m_batchDone = 0;
    m_batchRefused = 0;
    for (const CatalogModel& model : catalog.models)
    {
        if (model.type != NO_TYPE && HotReload::CanReload(model.type) && QueueReload(model, variant))
            ++m_batchTotal;
    }
    m_status = "Loading " + std::to_string(m_batchTotal) + " models (" + VariantName(variant) + ")...";
}

void CMapAssetVariants::RenderAllModelsSwitch(int world, const Catalog& catalog)
{
    EnsureWorld(world);
    TakeOutcomes();
    RefreshShown(catalog);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("All models:");
    ImGui::SameLine();
    ImGui::BeginDisabled(IsBusy());
    if (ImGui::Button("Current##AllModels"))
        QueueAll(catalog, AssetVariant::Current);
    HoverTip("Loads every model of the list again from src/bin/Data, with its textures. Also picks up files "
             "that changed on disk since the map was loaded.");
    ImGui::SameLine();
    ImGui::BeginDisabled(m_originalState == VariantState::Missing);
    if (ImGui::Button("Original##AllModels"))
        QueueAll(catalog, AssetVariant::Original);
    HoverTip("Loads every model of the list from out/ab/original: the files from before the art rebuild.");
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (IsBusy())
    {
        ImGui::SameLine();
        ImGui::Text("loading %d of %d...", m_batchDone, m_batchTotal);
    }
    RenderOriginalFilesHint();
    Editor::StatusLine::Render(m_status);
}

void CMapAssetVariants::RenderOriginalFilesHint()
{
    if (m_originalState == VariantState::Ready)
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, Editor::StatusLine::WarningColor());
    ImGui::TextWrapped(m_originalState == VariantState::Missing
                           ? "The original files are not built yet (out/ab/original). Run this in a terminal:"
                           : "out/ab/original was built from an older catalog.json. Run this in a terminal again:");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##MaterializeCommand", m_command.data(), m_command.size() + 1, ImGuiInputTextFlags_ReadOnly);
    if (ImGui::Button("Copy command"))
        ImGui::SetClipboardText(m_command.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Check again"))
        CheckOriginalFiles();
}

void CMapAssetVariants::RenderModelSwitch(const CatalogModel& model)
{
    if (model.type == NO_TYPE || !HotReload::CanReload(model.type))
    {
        ImGui::TextDisabled("Not a model loaded on this map: no current/original switch.");
        return;
    }
    const Shown shown = ShownOf(model);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Shows:");
    ImGui::SameLine();
    ImGui::TextColored(shown == Shown::Original ? COLOR_ORIGINAL : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s",
                       ShownLabel(model));
    if (shown == Shown::Mixed)
        HoverTip("The model file and its textures come from different variants: a texture it shares with another "
                 "model shows the variant that model was switched to last.");
    if (shown == Shown::AsBuilt)
        HoverTip("The files the map load read from the game's own Data folder, which the last build copied from "
                 "src/bin/Data. Files pulled or delivered since then only show after Current or a new build.");
    ImGui::SameLine();
    ImGui::BeginDisabled(IsBusy());
    if (ImGui::RadioButton("Current", shown == Shown::Current))
        QueueOne(model, AssetVariant::Current);
    ImGui::SameLine();
    ImGui::BeginDisabled(m_originalState == VariantState::Missing);
    if (ImGui::RadioButton("Original", shown == Shown::Original))
        QueueOne(model, AssetVariant::Original);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reload from disk"))
        QueueOne(model, HotReload::LoadedVariant(model.type).value_or(AssetVariant::Current));
    HoverTip("Loads the model and its textures again from the files it shows now (src/bin/Data, or "
             "out/ab/original), for example after Codex delivered new files.");
    ImGui::EndDisabled();
    if (shown == Shown::AsBuilt)
        ImGui::TextDisabled("As built: from the last build. Before a verdict on new files, click Current.");
    RenderSharedTextureNote(model);
}

void CMapAssetVariants::RenderSharedTextureNote(const CatalogModel& model)
{
    std::set<std::string> partners;
    for (const TextureLink& texture : model.textures)
        partners.insert(texture.sharedWith.begin(), texture.sharedWith.end());
    if (partners.empty())
        return;
    std::string names;
    for (const std::string& partner : partners)
        names += (names.empty() ? "" : ", ") + partner;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Its textures are shared with %s: they switch too.", names.c_str());
    ImGui::PopStyleColor();
}

#endif // _EDITOR
