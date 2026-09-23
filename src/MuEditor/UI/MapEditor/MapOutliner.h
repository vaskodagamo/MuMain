#pragma once

#ifdef _EDITOR

#include <string>
#include <vector>

class OBJECT;

// The Map Editor's "Outliner" window: every live world object of the loaded map with
// its name (the asset catalog's, else the model's own), type, position in tiles,
// angle and scale; a name filter and a type filter. A click selects the object and
// points the free-fly camera at it, Shift/Cmd+click adds or removes it, and "Select
// all of type" selects every instance of a model. The selection is the Objects tab's.
class CMapOutliner
{
public:
    static CMapOutliner& GetInstance();

    // Draws the window while `*open`; `world` is the map folder the names come from.
    // True when a click in the window changed the selection this frame.
    bool Render(bool* open, int world);

private:
    CMapOutliner() = default;

    void RefreshNames(int world);
    void RefreshNameMatches();
    void CollectRows();
    void RenderFilters();
    void RenderTypeFilter();
    void RenderTable();
    void RenderRow(OBJECT* object);
    void OnRowClicked(OBJECT* object);
    void SelectAllOfType(int type);
    const std::string& NameOf(int type) const;

    int m_world = -1;
    std::vector<std::string> m_typeNames; // by model type
    std::vector<char> m_nameMatches;      // by model type: the name contains the filter text
    std::vector<int> m_typeCounts;        // live objects by model type, this frame
    char m_filter[64] = {};
    std::string m_appliedFilter; // the filter text m_nameMatches was built for
    int m_typeFilter = -1;       // -1: every type
    std::vector<OBJECT*> m_rows; // the objects the filters let through, this frame
    bool m_selected = false;     // a click changed the selection this frame
};

#define g_MapOutliner CMapOutliner::GetInstance()

#endif // _EDITOR
