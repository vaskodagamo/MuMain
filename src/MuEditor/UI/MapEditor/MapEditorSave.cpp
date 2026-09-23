#include "stdafx.h"

#ifdef _EDITOR

#include "MapEditorSave.h"
#include "MapEditorFileUtil.h"

#include "Render/Terrain/ZzzLodTerrain.h"   // terrain arrays + MapFileEncrypt + TERRAIN_SIZE
#include "UI/Console/MuEditorConsoleUI.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace Editor::MapSave
{
namespace
{
    // Bytes-per-cell for each mapping array in the .map layout.
    constexpr int CELLS = TERRAIN_SIZE * TERRAIN_SIZE;

    // .map plaintext layout (matches OpenTerrainMapping in ZzzLodTerrain.cpp):
    //   byte version, byte mapNumber, byte Layer1[CELLS], byte Layer2[CELLS], byte Alpha[CELLS]
    constexpr int HEADER_BYTES = 2;
    constexpr int PLAIN_BYTES  = HEADER_BYTES + CELLS * 3;

    constexpr BYTE MAP_VERSION = 0;
}

bool SaveMappingEncrypted(int worldNumber, int mapNumber, std::string& outReport)
{
    // Build the decrypted byte image exactly as the loader expects to read it.
    auto plain = std::make_unique<BYTE[]>(PLAIN_BYTES);
    int p = 0;
    plain[p++] = MAP_VERSION;
    plain[p++] = static_cast<BYTE>(mapNumber);

    std::memcpy(&plain[p], TerrainMappingLayer1, CELLS);
    p += CELLS;
    std::memcpy(&plain[p], TerrainMappingLayer2, CELLS);
    p += CELLS;
    for (int i = 0; i < CELLS; ++i)
        plain[p++] = static_cast<BYTE>(TerrainMappingAlpha[i] * 255.f);

    // Encrypt with the same rolling-XOR cipher the loader decrypts with.
    auto enc = std::make_unique<BYTE[]>(PLAIN_BYTES);
    const int encBytes = MapFileEncrypt(enc.get(), plain.get(), PLAIN_BYTES);

    const std::filesystem::path fileName = Editor::Files::TerrainMappingFile(worldNumber);

    const std::string target = Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(fileName));
    FILE* fp = _wfopen(fileName.wstring().c_str(), L"wb");
    if (fp == nullptr)
    {
        outReport = "Save FAILED: could not open " + target + " for writing.";
        g_MuEditorConsoleUI.LogEditor("[MapEditor] SaveMapping FAILED: could not open " + target);
        return false;
    }

    const bool ok = fwrite(enc.get(), 1, encBytes, fp) == static_cast<size_t>(encBytes);
    const bool closed = fclose(fp) == 0;

    if (!ok || !closed)
    {
        outReport = "Save FAILED: write error (disk full?) in " + target + ".";
        g_MuEditorConsoleUI.LogEditor("[MapEditor] SaveMapping FAILED: write error in " + target);
        return false;
    }

    outReport = Editor::Files::DescribeSavedFiles({Editor::Files::MirrorSavedFile(fileName)});
    return true;
}

} // namespace Editor::MapSave

#endif // _EDITOR
