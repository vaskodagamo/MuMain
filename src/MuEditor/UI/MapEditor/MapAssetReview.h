#pragma once

#ifdef _EDITOR

#include "Assets/AssetCatalog.h"
#include "Assets/ClientReview.h"

#include <string>
#include <vector>

class OBJECT;

// The Map Editor's "Assets" tab: every model of the world's asset catalog
// (assets-work/World{N}/catalog.json) with its identity, rebuild status, batches,
// textures and engine rules; how often it is placed on the loaded map; buttons to
// outline and visit its instances; the owner's quick client verdicts; and "Flag
// for regeneration...", which files a request for the art builder (see
// CRegenRequestDialog). The Objects tab shows the same facts for the selected object.
class CMapAssetReview
{
public:
    static CMapAssetReview& GetInstance();

    // Draws the tab. `selected` is the Map Editor's selected object: stepping to an
    // instance selects it.
    void Render(int world, OBJECT*& selected);

    // Objects tab: the catalog facts of `selected` with "Flag for regeneration..."
    // and "Show in Assets tab". Returns true when the owner asked for the Assets tab.
    bool RenderObjectSummary(int world, const OBJECT* selected);

    // Stops outlining instances (the Map Editor closed).
    void ClearHighlight();

    // The catalog's name of model `type` on `world` (read when the world changed), or
    // nullptr when the catalog does not list the type or there is no catalog.
    const std::string* CatalogNameOf(int world, int type);

private:
    CMapAssetReview() = default;

    // Loads catalog.json and client-review.json when the world changed (or on Reload).
    void EnsureCatalog(int world);
    const Editor::Assets::CatalogModel* SelectedModel() const;
    // The owner's verdict: client-review.json when it has one, else the catalog's.
    const Editor::Assets::ClientReview* VerdictOf(const Editor::Assets::CatalogModel& model) const;
    bool RenderCatalogProblem(int world);
    void RenderFilters();
    void RebuildRows();
    void SortRows(unsigned int column, bool ascending);
    int LiveCount(int type) const;
    void RenderTable();
    void RenderTableRow(int index);
    void RenderDetails(OBJECT*& selected);
    void RenderInstanceActions(const Editor::Assets::CatalogModel& model, OBJECT*& selected);
    void RenderVerdict(const Editor::Assets::CatalogModel& model);
    void RenderTextures(const Editor::Assets::CatalogModel& model);
    void RenderHistory(const Editor::Assets::CatalogModel& model);
    void StepInstance(const Editor::Assets::CatalogModel& model, int step, OBJECT*& selected);
    void RecordVerdict(const Editor::Assets::CatalogModel& model, const char* verdict);
    void OpenFlagDialog(const Editor::Assets::CatalogModel& model, const OBJECT* picked);
    void NoteFiledRequests();
    void OpenRepoFile(const std::string& repoPath);
    void UpdateHighlight();

    int m_world = -1;
    Editor::Assets::CatalogLoad m_load;
    Editor::Assets::ClientReviews m_reviews;
    std::string m_reviewError;

    char m_filter[64] = {};
    int m_statusFilter = 0;        // index into the status filter list
    std::vector<int> m_rows;       // catalog model indices shown in the table, filtered and sorted
    std::vector<int> m_liveCounts; // live objects per type on the loaded map, refreshed each frame
    std::string m_selectedName;    // model shown in the details
    bool m_scrollToSelected = false;
    int m_instanceIndex = -1; // last instance Prev/Next visited
    bool m_highlight = false; // "Highlight all": outline every instance of the selected model
    char m_verdictNote[256] = {};
    std::string m_status;
};

#define g_MapAssetReview CMapAssetReview::GetInstance()

#endif // _EDITOR
