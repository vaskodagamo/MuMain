#pragma once

#ifdef _EDITOR

#include "MapBrushControls.h" // TerrainBrushInput

#include "Gates/GateTableEdit.h"

#include <string>
#include <utility>
#include <vector>

// The Map Editor's Gates tab: the gates of Data/Gate.bmd on the loaded map, drawn on the
// ground over the walkability overlay; an area drawn with the mouse becomes a new gate to
// another map (or changes a gate the editor added); every change is saved to Gate.bmd at
// once. Also writes the map's OpenMU export.
class CMapGatesTab
{
public:
    // Draws the tab. Returns the edit mode it needs this frame: EDIT_WALL while an area
    // is being drawn (the ground under the cursor), else EDIT_NONE.
    int Render(const TerrainBrushInput& input);

    // Selects gate `number` of the loaded map (the control socket's gate-show).
    void Select(int number);

private:
    struct MapChoice
    {
        int map = 0;
        std::string label; // "82 Lorencia Outskirts (World83)"
    };

    void ResetForMap(int map);
    // The maps a gate can lead to: every Data/World folder of the game.
    void RefreshMaps();
    void RenderHeader(int map);
    void RenderGateTable(const Editor::Gates::GateTable& table, int map);
    void RenderWaysIn(const Editor::Gates::GateTable& table, int map);
    void RenderDrawing(const TerrainBrushInput& input);
    void TrackDrag(const TerrainBrushInput& input);
    void RenderNewGate(int map);
    void RenderTargetMap();
    void RenderSelected(const Editor::Gates::GateTable& table);
    void RenderExport(int map);
    void ShowOnGround(const Editor::Gates::GateTable& table, int map) const;
    void ShowResult(const std::string& action, bool ok, const std::string& error, const std::string& report,
                    const std::vector<std::string>& warnings);
    void LookAt(const Editor::Gates::TileRect& area);

    int m_map = -1;                  // the map the tab's state belongs to
    int m_selected = -1;             // a gate number, -1 for none
    bool m_scrollToSelected = false; // gate-show: bring the selected row into view
    bool m_drawing = false;
    bool m_dragging = false;
    int m_dragStartX = 0;
    int m_dragStartY = 0;
    bool m_hasDrawn = false;
    Editor::Gates::TileRect m_drawn;

    // The next gate: from m_drawn on this map to m_targetArea on m_targetMap.
    int m_targetMap = 0;
    int m_targetArea[4] = {};
    int m_direction = 0;
    int m_level = 0;
    std::vector<MapChoice> m_maps;

    std::string m_status;
    std::vector<std::string> m_warnings;
};

#endif // _EDITOR
