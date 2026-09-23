#pragma once

#ifdef _EDITOR

#include "MapInspect/MapDigest.h"           // SaveUnit
#include "UI/MapEditor/MapEditorFileUtil.h" // SavedFile

#include <string>
#include <vector>

// Saving the loaded map's files and reading them back (the control socket's map-save
// and map-revert), through the Map Editor's own saves.
namespace Editor::LiveMapFiles
{
using Editor::MapInspect::SaveUnit;

// What saving one file did.
struct SavedUnit
{
    SaveUnit unit = SaveUnit::Texture;
    bool saved = false;
    std::string report;            // the Map Editor's status line: what was written, or why not
    Editor::Files::SavedFile file; // where it went (runtime Data, repository copy, backup)
};

// Saves `units` of the loaded map to Data/World{N} with the Map Editor's saves, each also
// copied into the repository's src/bin/Data with a backup of the file it replaces.
std::vector<SavedUnit> Save(const std::vector<SaveUnit>& units);

// Reads `units` of the loaded map back from its files in Data/World{N}, throwing away
// what was not saved: the terrain layers are read the way the map load reads them
// (and relit), the objects the file places are created again. Every file is checked
// before anything changes; false with the reason (and nothing reverted) when one cannot
// be read. The Map Editor's undo history, held strokes and selection are dropped, as
// after a map load.
bool Revert(const std::vector<SaveUnit>& units, std::string& error);
} // namespace Editor::LiveMapFiles

#endif // _EDITOR
