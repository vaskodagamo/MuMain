#pragma once

#ifdef _EDITOR

#include "MapEditorFileUtil.h" // SavedFile

#include <string>

// Saves the map's heights (the Height tab's Save height, the control socket's map-save).
namespace Editor::HeightSave
{
// Writes Data/World{world}/TerrainHeight.OZB with the engine's SaveTerrainHeight and
// copies it into the repository (Editor::Files::MirrorSavedFile). Refused on maps whose
// height file stores 24 bits per corner, which that save would overwrite with 8-bit
// heights. `outReport` gets the status-line text; `outSaved`, when given, where the file
// went. False when nothing was written.
bool Save(int world, std::string& outReport, Editor::Files::SavedFile* outSaved = nullptr);
} // namespace Editor::HeightSave

#endif // _EDITOR
