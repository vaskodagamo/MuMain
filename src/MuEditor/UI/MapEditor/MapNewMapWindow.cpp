#include "stdafx.h"

#ifdef _EDITOR

#include "MapNewMapWindow.h"

#include "MapEditorStatusLine.h"

#include "Core/LiveGates.h"
#include "Core/NewMapFiles.h"
#include "Core/OfflineWorld.h"
#include "MapInspect/TilePalette.h"
#include "MapScript/AttributeRules.h"

#include "World/MapInfra/MapNumbers.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
namespace NewMap = Editor::NewMap;
namespace Attributes = Editor::MapScript::Attributes;
namespace Numbers = World::MapNumbers;

constexpr float HEIGHT_STEP = 1.5f; // TerrainHeight.OZB stores height / 1.5 in one byte
constexpr float MAX_HEIGHT = 255 * HEIGHT_STEP;
constexpr int NO_WORLD = 0;
constexpr std::array<std::uint16_t, 5> WALK_VALUES = {Attributes::WALKABLE, Attributes::SAFEZONE, Attributes::BLOCKED,
                                                      Attributes::VOID_GROUND, Attributes::WATER};
const ImVec2 WINDOW_SIZE(460.0f, 520.0f);
const ImVec4 NUMBER_COLOR(0.9f, 0.9f, 0.6f, 1.0f);

std::string WorldLabel(int world)
{
    const int map = Numbers::MapOfFolder(world);
    return "World" + std::to_string(world) + ": " + Editor::LiveGates::MapName(map) + " (map " + std::to_string(map) +
           ")";
}
} // namespace

void CMapNewMapWindow::Render(bool* open)
{
    if (open == nullptr || !*open)
    {
        m_refreshed = false;
        return;
    }
    if (!m_refreshed)
        Refresh();
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("New map", open))
    {
        ImGui::End();
        return;
    }
    RenderNumberAndName();
    RenderSource();
    RenderModels();
    RenderActions();
    ImGui::End();
}

void CMapNewMapWindow::Refresh()
{
    m_refreshed = true;
    m_worlds = Editor::NewMapFiles::WorldFolders();
    m_labels.clear();
    for (const int world : m_worlds)
        m_labels.push_back(WorldLabel(world));
    const int next = Editor::NewMapFiles::NextFreeMapNumber();
    m_map = next >= 0 ? next : Numbers::FIRST_NEW_MAP;
}

void CMapNewMapWindow::RenderNumberAndName()
{
    ImGui::InputInt("Map number", &m_map);
    m_map = std::clamp(m_map, Numbers::FIRST_NEW_MAP, Numbers::LAST_NEW_MAP);
    ImGui::TextColored(NUMBER_COLOR, "OpenMU and Gate.bmd: map Number %d.  Client folder: Data/World%d.", m_map,
                       Numbers::FolderOf(m_map));
    ImGui::TextDisabled("Offline it opens with --world %d (the folder number).", Numbers::FolderOf(m_map));
    ImGui::InputText("Name", m_name, sizeof(m_name));
    ImGui::Separator();
}

void CMapNewMapWindow::RenderSource()
{
    int source = m_template ? 1 : 0;
    ImGui::RadioButton("Flat", &source, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Copy of a map", &source, 1);
    if ((source == 1) != m_template)
    {
        m_template = source == 1;
        m_modelsWorld = m_template ? m_templateWorld : NO_WORLD;
    }
    if (m_template)
        RenderTemplate();
    else
        RenderBlank();
}

void CMapNewMapWindow::RenderTemplate()
{
    if (WorldCombo("Copy", m_templateWorld, false))
        m_modelsWorld = m_templateWorld;
    ImGui::Checkbox("Copy its minimap too", &m_copyMinimap);
}

void CMapNewMapWindow::RenderBlank()
{
    ImGui::SliderFloat("Height", &m_height, 0.0f, MAX_HEIGHT, "%.1f");
    const std::string slotName = Editor::MapInspect::TileSlotName(m_tileSlot);
    if (ImGui::BeginCombo("Texture", slotName.c_str()))
    {
        for (int slot = 0; slot < Editor::MapInspect::TILE_SLOT_COUNT; ++slot)
        {
            if (ImGui::Selectable(Editor::MapInspect::TileSlotName(slot).c_str(), slot == m_tileSlot))
                m_tileSlot = slot;
        }
        ImGui::EndCombo();
    }
    const std::string walkName = Attributes::NameOf(WALK_VALUES[static_cast<std::size_t>(m_attributeChoice)]);
    if (ImGui::BeginCombo("Walkability", walkName.c_str()))
    {
        for (std::size_t choice = 0; choice < WALK_VALUES.size(); ++choice)
        {
            if (ImGui::Selectable(Attributes::NameOf(WALK_VALUES[choice]).c_str(),
                                  static_cast<int>(choice) == m_attributeChoice))
                m_attributeChoice = static_cast<int>(choice);
        }
        ImGui::EndCombo();
    }
    ImGui::SliderFloat("Light", &m_light, 0.0f, 1.0f, "%.2f");
    WorldCombo("Textures from", m_texturesWorld, false);
}

void CMapNewMapWindow::RenderModels()
{
    WorldCombo("Models from", m_modelsWorld, true);
    ImGui::TextDisabled("Lorencia's models are renamed to the numbered files other maps use.");
    ImGui::Separator();
}

void CMapNewMapWindow::RenderActions()
{
    const bool check = ImGui::Button("Check");
    ImGui::SameLine();
    const bool create = ImGui::Button("Create map");
    if (check || create)
    {
        Editor::NewMapFiles::CreateResult result;
        std::string error;
        const bool ok = Editor::NewMapFiles::Create(Request(), check, result, error);
        m_status = ok ? result.report : "Not created: " + error;
        if (ok && create)
        {
            m_createdWorld = result.plan.world;
            Refresh(); // the new folder is a choice now, and the next free number moves on
        }
    }
    if (m_createdWorld != NO_WORLD && Editor::OfflineWorld::IsActive())
    {
        ImGui::SameLine();
        if (ImGui::Button("Open it now"))
        {
            std::string error;
            if (!Editor::OfflineWorld::Open(m_createdWorld, error))
                m_status = "Could not open it: " + error;
        }
    }
    Editor::StatusLine::Render(m_status);
}

bool CMapNewMapWindow::WorldCombo(const char* label, int& world, bool allowNone)
{
    const auto found = std::find(m_worlds.begin(), m_worlds.end(), world);
    const std::string preview =
        found != m_worlds.end() ? m_labels[static_cast<std::size_t>(found - m_worlds.begin())] : std::string("none");
    if (!ImGui::BeginCombo(label, preview.c_str()))
        return false;
    bool changed = false;
    if (allowNone && ImGui::Selectable("none", world == NO_WORLD))
    {
        world = NO_WORLD;
        changed = true;
    }
    for (std::size_t i = 0; i < m_worlds.size(); ++i)
    {
        if (ImGui::Selectable(m_labels[i].c_str(), m_worlds[i] == world))
        {
            world = m_worlds[i];
            changed = true;
        }
    }
    ImGui::EndCombo();
    return changed;
}

NewMap::NewMapRequest CMapNewMapWindow::Request() const
{
    NewMap::NewMapRequest request;
    request.map = m_map;
    request.name = m_name;
    request.source = m_template ? NewMap::MapSource::Template : NewMap::MapSource::Blank;
    request.templateWorld = m_templateWorld;
    request.blank.heightByte = static_cast<std::uint8_t>(std::lround(m_height / HEIGHT_STEP));
    request.blank.tileSlot = static_cast<std::uint8_t>(m_tileSlot);
    request.blank.attribute = static_cast<std::uint8_t>(WALK_VALUES[static_cast<std::size_t>(m_attributeChoice)]);
    request.blank.light = {m_light, m_light, m_light};
    request.texturesWorld = m_texturesWorld;
    request.modelsWorld = m_modelsWorld;
    request.copyMinimap = m_copyMinimap;
    return request;
}

#endif // _EDITOR
