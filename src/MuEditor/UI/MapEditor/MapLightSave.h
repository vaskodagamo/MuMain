#pragma once

#ifdef _EDITOR

#include "MapEditorFileUtil.h" // SavedFile

#include <string>

// Saves the map's painted light map (TerrainLight) the way the client loads it.
namespace Editor::LightSave
{
// Writes Data/World{world}/TerrainLight.OZJ (a 256 x 256 JPEG behind the 24-byte
// .OZJ prefix) and copies it into the repository (Editor::Files::MirrorSavedFile).
// A JPEG loses a little, so the file is then read back as a map load reads it and
// the ground is lit from that: what the editor shows after a save is what the next
// load shows. Returns false when the file could not be written or read back.
// `outReport` gets the status-line text; `outSaved`, when given, where the file went.
bool Save(int world, std::string& outReport, Editor::Files::SavedFile* outSaved = nullptr);
} // namespace Editor::LightSave

#endif // _EDITOR
