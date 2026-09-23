#include "stdafx.h"

#ifdef _EDITOR

#include "MapLightSave.h"

#include "MapEditorFileUtil.h"

#include "Assets/TerrainLightFile.h"
#include "Render/Terrain/ZzzLodTerrain.h" // TerrainLight, CreateTerrainLight
#include "UI/Console/MuEditorConsoleUI.h"

#include <cstdio>
#include <filesystem>
#include <vector>

namespace Editor::LightSave
{
namespace
{
bool WriteFile(const std::filesystem::path& file, const std::vector<std::uint8_t>& bytes)
{
    FILE* fp = _wfopen(file.wstring().c_str(), L"wb");
    if (fp == nullptr)
        return false;
    const bool written = fwrite(bytes.data(), 1, bytes.size(), fp) == bytes.size();
    const bool closed = fclose(fp) == 0;
    return written && closed;
}

bool Fail(const std::string& reason, std::string& outReport)
{
    outReport = "Save light FAILED: " + reason;
    g_MuEditorConsoleUI.LogEditor("[MapEditor] " + outReport);
    return false;
}
} // namespace

bool Save(int world, std::string& outReport)
{
    static_assert(TERRAIN_SIZE == Editor::LightMap::LIGHT_MAP_SIZE, "the light map is one pixel per terrain corner");
    const std::filesystem::path file = Editor::Files::TerrainLightFile(world);
    const std::string target = Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(file));

    const std::vector<std::uint8_t> bytes = Editor::LightMap::EncodeOzj(&TerrainLight[0][0], TERRAIN_SIZE);
    if (bytes.empty())
        return Fail("the JPEG encoder failed; " + target + " is unchanged.", outReport);
    if (!WriteFile(file, bytes))
        return Fail("could not write " + target + " (disk full?); it may be truncated.", outReport);

    // Light the ground from what the file holds, as the next map load will.
    std::string error;
    if (!Editor::LightMap::DecodeOzj(Editor::Files::ReadWholeFile(file), TERRAIN_SIZE, &TerrainLight[0][0], error))
        return Fail("could not read back " + target + ": " + error, outReport);
    CreateTerrainLight();

    outReport = Editor::Files::DescribeSavedFiles({Editor::Files::MirrorSavedFile(file)});
    return true;
}
} // namespace Editor::LightSave

#endif // _EDITOR
