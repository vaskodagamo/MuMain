#pragma once

#ifdef _EDITOR

#include "Core/EditorFiles.h"

#include <filesystem>

// The Map Editor's folders and files in the client data tree; the save, mirror
// and read helpers every editor shares are in Core/EditorFiles.h.
namespace Editor::Files
{
std::filesystem::path WorldDir(int world);  // Data/World{world}
std::filesystem::path ObjectDir(int world); // Data/Object{world}

// The map files the game loads from WorldDir(world) and the Map Editor saves.
std::filesystem::path TerrainMappingFile(int world);   // Data/World{world}/EncTerrain{world}.map
std::filesystem::path TerrainAttributeFile(int world); // Data/World{world}/EncTerrain{world}.att
std::filesystem::path TerrainObjectFile(int world);    // Data/World{world}/EncTerrain{world}.obj
std::filesystem::path TerrainHeightFile(int world);    // Data/World{world}/TerrainHeight.OZB
std::filesystem::path TerrainLightFile(int world);     // Data/World{world}/TerrainLight.OZJ
} // namespace Editor::Files

#endif // _EDITOR
