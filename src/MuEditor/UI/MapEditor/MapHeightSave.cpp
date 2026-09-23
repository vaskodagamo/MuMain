#include "stdafx.h"

#ifdef _EDITOR

#include "MapHeightSave.h"

#include "Core/LiveMap.h"
#include "Render/Terrain/ZzzLodTerrain.h" // SaveTerrainHeight, IsTerrainHeightExtMap
#include "UI/Console/MuEditorConsoleUI.h"
#include "World/MapInfra/MapManager.h" // gMapManager.WorldActive

#include <filesystem>

namespace Editor::HeightSave
{
bool Save(int world, std::string& outReport, Editor::Files::SavedFile* outSaved)
{
    const std::filesystem::path fileName = Editor::Files::TerrainHeightFile(world);
    const std::string target = Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(fileName));
    if (IsTerrainHeightExtMap(gMapManager.WorldActive))
    {
        outReport = "Save refused: this map's " + target +
                    " stores 24-bit heights, and the height save writes 8-bit ones; the file is unchanged.";
        return false;
    }
    std::wstring saveName = fileName.wstring(); // SaveTerrainHeight takes a mutable buffer
    if (!SaveTerrainHeight(saveName.data()))
    {
        g_MuEditorConsoleUI.LogEditor("[MapEditor] FAILED to save terrain height (disk full / I/O error?)");
        outReport = "Save failed - " + target + " may be truncated or missing.";
        return false;
    }
    const Editor::Files::SavedFile saved = Editor::Files::MirrorSavedFile(fileName);
    outReport = Editor::Files::DescribeSavedFiles({saved});
    if (outSaved != nullptr)
        *outSaved = saved;
    Editor::LiveMap::NoteSaved(Editor::MapInspect::SaveUnit::Height, world);
    return true;
}
} // namespace Editor::HeightSave

#endif // _EDITOR
