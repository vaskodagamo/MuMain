#pragma once

#ifdef _EDITOR

#include "NewMap/NewMapPlan.h"

#include "World/MapInfra/CustomMapName.h"

#include <string>
#include <vector>

// The Map Editor's "New map" window: a new map number (82 to 254, shown with its client
// folder), a name, and either a copy of another map or a flat map; it writes the new
// map's folders into the game's Data and the repository, and opens it in an offline
// session.
class CMapNewMapWindow
{
public:
    // Draws the window while `*open`.
    void Render(bool* open);

private:
    void Refresh();
    void RenderNumberAndName();
    void RenderSource();
    void RenderTemplate();
    void RenderBlank();
    void RenderModels();
    void RenderActions();
    // A combo of the game's map folders; `allowNone` adds "none" (0).
    bool WorldCombo(const char* label, int& world, bool allowNone);
    Editor::NewMap::NewMapRequest Request() const;

    bool m_refreshed = false;
    std::vector<int> m_worlds; // Data/World folders of the game
    std::vector<std::string> m_labels;

    int m_map = 0;
    char m_name[World::MapNames::MAX_NAME_BYTES + 1] = {};
    bool m_template = false;
    int m_templateWorld = 1;
    float m_height = 150.0f;
    int m_tileSlot = 0;
    int m_attributeChoice = 0;
    float m_light = 1.0f;
    int m_texturesWorld = 1;
    int m_modelsWorld = 0;
    bool m_copyMinimap = true;

    int m_createdWorld = 0; // the folder created last, for "Open it now"
    std::string m_status;
};

#endif // _EDITOR
