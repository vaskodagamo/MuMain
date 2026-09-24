#pragma once

#ifdef _EDITOR

#include "Gates/GateChecks.h"
#include "Gates/GateTableEdit.h"
#include "UI/MapEditor/MapEditorFileUtil.h" // SavedFile

#include <string>
#include <vector>

// The gate table the client has loaded (GateAttribute, read from Data/Gate.bmd at start)
// as the gate units see it, and the edits the Gates tab and the control socket's gate-*
// commands make to it. Every edit is saved to Data/Gate.bmd at once (and copied into the
// repository with a backup): the file is one table for every map, not part of a map's
// undo history.
namespace Editor::LiveGates
{
Editor::Gates::GateTable Table();

// Takes Data/Gate.bmd as it is on disk as the client's table. Another client run from the
// same folder, or a checkout, may have changed the file since this one read it; an edit
// writes the whole table, so one based on the old copy in memory would erase the other
// client's gates and hand their numbers out again. Every edit and export calls it first;
// `notes` gets a sentence when the file had changed. False with the reason, and nothing
// changed, when the file is there but is not a gate table (a missing one is written from
// memory by the next edit).
bool Refresh(std::vector<std::string>& notes, std::string& error);

// The walk map of `map`: the loaded map's own (with its unsaved edits), else the client
// .att in Data/World{map + 1}.
Editor::Gates::WalkMap WalkMapOf(int map);
// The client .att in Data/World{map + 1} as saved, also for the loaded map.
Editor::Gates::WalkMap SavedWalkMapOf(int map);

// The game's name of `map` (UTF-8), as the client shows it.
std::string MapName(int map);
// Data/World{map + 1} exists (maps whose event levels share a folder: the shared one).
bool MapHasFolder(int map);

// What an edit did.
struct EditResult
{
    bool saved = false;
    std::vector<int> numbers; // added (enter, then arrival) or removed
    std::vector<std::string> warnings;
    Editor::Files::SavedFile file; // Data/Gate.bmd, when saved
    std::string report;            // the status line: what was written
};

// Adds `pair` (checked: its maps must have folders; walkability and gates in the way are
// warnings) and saves Gate.bmd, or with `dryRun` only reports what it would do. False with
// the reason, and nothing changed, when the pair cannot be added or the save failed.
bool AddPair(const Editor::Gates::NewGatePair& pair, bool dryRun, EditResult& result, std::string& error);

// Removes gate `number` (see Editor::Gates::RemoveGate) and saves Gate.bmd.
bool Remove(int number, bool dryRun, EditResult& result, std::string& error);

// Changes gate `number` (see Editor::Gates::ChangeGate) and saves Gate.bmd.
bool Change(int number, const Editor::Gates::GateChange& change, EditResult& result, std::string& error);
} // namespace Editor::LiveGates

#endif // _EDITOR
