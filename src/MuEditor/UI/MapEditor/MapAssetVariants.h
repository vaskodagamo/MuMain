#pragma once

#ifdef _EDITOR

#include "Assets/AssetCatalog.h"
#include "Assets/AssetVariant.h"

#include <filesystem>
#include <string>
#include <unordered_map>

// The Assets tab's A/B compare: which files each catalog model shows in the
// running client (the checkout's current ones, or the originals from before the
// art rebuild), a switch for one model and one for all of them, and "Reload from
// disk". Editor::Assets::HotReload does the loading between frames.
class CMapAssetVariants
{
public:
    static CMapAssetVariants& GetInstance();

    // The row under the catalog line: "All models: Current | Original", the
    // progress of a switch, its result, and the command that builds the original
    // files when they are missing or older than the catalog.
    void RenderAllModelsSwitch(int world, const Editor::Assets::Catalog& catalog);

    // In the details of `model`: what it shows, "Show: Current | Original" and
    // "Reload from disk".
    void RenderModelSwitch(const Editor::Assets::CatalogModel& model);

    // The table cell: "as built", "current", "original", "mixed", or "-" for a
    // model without a world-object type or not loaded on this map.
    const char* ShownLabel(const Editor::Assets::CatalogModel& model) const;

    // Checks the original files and what each model shows again on the next frame
    // (the catalog was read again, or the world changed).
    void Reset();

private:
    // What a model shows: what the map load read from the game's own Data folder
    // (the last build's copy of src/bin/Data, which can be older), its model file
    // and all its textures from one variant, or a mix (a texture shared with a
    // model switched later follows that model).
    enum class Shown
    {
        NotLoaded,
        AsBuilt,
        Current,
        Original,
        Mixed,
    };

    CMapAssetVariants() = default;

    void EnsureWorld(int world);
    void CheckOriginalFiles();
    void TakeOutcomes();
    void RefreshShown(const Editor::Assets::Catalog& catalog);
    Shown ShownFor(int type) const;
    Shown ShownOf(const Editor::Assets::CatalogModel& model) const;
    bool QueueReload(const Editor::Assets::CatalogModel& model, Editor::Assets::AssetVariant variant);
    void QueueOne(const Editor::Assets::CatalogModel& model, Editor::Assets::AssetVariant variant);
    void QueueAll(const Editor::Assets::Catalog& catalog, Editor::Assets::AssetVariant variant);
    void RenderOriginalFilesHint();
    void RenderSharedTextureNote(const Editor::Assets::CatalogModel& model);

    int m_world = -1;
    std::filesystem::path m_repo;
    std::filesystem::path m_originalRoot; // <repo>/out/ab/original/Data
    Editor::Assets::VariantState m_originalState = Editor::Assets::VariantState::Missing;
    std::string m_command;                  // MaterializeCommand, for the read-only field and Copy command
    std::unordered_map<int, Shown> m_shown; // by type
    int m_refreshFrame = -1;                // ImGui frame of the last RefreshShown; -1 = refresh now
    Editor::Assets::AssetVariant m_batchVariant = Editor::Assets::AssetVariant::Current;
    int m_batchTotal = 0; // requests of the running switch
    int m_batchDone = 0;
    int m_batchRefused = 0;
    std::string m_firstRefusal;
    std::string m_status;
};

#define g_MapAssetVariants CMapAssetVariants::GetInstance()

#endif // _EDITOR
