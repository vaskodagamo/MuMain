#pragma once

#ifdef _EDITOR

#include "NewMap/NewMapPlan.h"
#include "NewMap/NewMapWriter.h"

#include <string>
#include <vector>

// Creating a new map in the running game's Data folder (and the repository's src/bin/Data),
// for the Map Editor's New map window and the control socket's map-new.
namespace Editor::NewMapFiles
{
struct CreateResult
{
    Editor::NewMap::NewMapPlan plan;                  // what was (or, in a dry run, would be) written
    std::vector<Editor::NewMap::WrittenFile> written; // empty in a dry run
    std::string report;                               // the status line
};

// The engine's ciphers for EncTerrain files.
Editor::NewMap::MapFileCodec EngineCodec();

// Plans the map and, unless `dryRun`, writes it. False with the reason, and nothing
// written, when the request is not valid, a source file is missing or a folder exists.
bool Create(const Editor::NewMap::NewMapRequest& request, bool dryRun, CreateResult& result, std::string& error);

// The numbers N of the Data/World{N} folders of the running game.
std::vector<int> WorldFolders();

// The lowest new map number whose folders are free, -1 when none is.
int NextFreeMapNumber();
} // namespace Editor::NewMapFiles

#endif // _EDITOR
