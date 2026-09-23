#pragma once

#ifdef _EDITOR

#include "Editing/CommandStack.h" // StepResult
#include "MapScript/EditScript.h"
#include "MapScript/OpReport.h"
#include "MapScript/ScriptDiff.h"
#include "MapScript/ScriptMap.h"

#include <cstddef>
#include <string>
#include <vector>

// Edit scripts on the map the client has loaded (the control socket's map-apply,
// map-undo and map-redo): the loaded map handed to MapScript/ as plain arrays, and what a
// script changed made on the live map as one step of the Map Editor's undo history.
namespace Editor::LiveMapEdit
{
// The loaded map as an edit script sees it: its terrain and saved objects (keyed by
// OBJECT::SaveOrder, in save order, so an object's id is the index map-query reports),
// and what it offers (loaded models and textures, its limits).
struct Snapshot
{
    Editor::MapScript::MapState state;
    Editor::MapScript::MapContext context;
};
Snapshot TakeSnapshot();

// The models the loaded map has (what scripts may place, and what its objects use), each
// with its model name and asset catalog name.
std::vector<Editor::MapScript::ModelInfo> LoadedModels();

// What map-apply did or, for a dry run, would do.
struct ApplyResult
{
    std::string label;
    std::vector<Editor::MapScript::OpReport> reports;
    Editor::MapScript::MapChanges changes;
    Editor::MapScript::MapContext context; // names the models in the report
    bool applied = false;                  // one undo step was added
};

// Runs `script` on a copy of the loaded map. Unless `dryRun`, what it changed is then
// made on the live map (terrain relit, objects created, moved or deleted) as one undo
// step named after the script; a script that changed nothing adds no step. False with
// the reason when the script is refused; the map is then as it was.
bool Apply(const Editor::MapScript::EditScript& script, bool dryRun, ApplyResult& result, std::string& error);

// A Map Editor stroke or drag is still held: scripted edits and steps wait for it.
bool IsEditHeld();

// The shared history's steps: undo steps oldest first (the last is the next undo), redo
// steps next first, and the memory they keep.
struct HistoryLabels
{
    std::vector<std::string> undo;
    std::vector<std::string> redo;
    std::size_t memoryBytes = 0;
};
HistoryLabels History();

// map-undo and map-redo: one step of the shared history (also the Map Editor's own
// steps). `label` gets the step's name. The Map Editor's selection is kept; objects the
// step deleted drop out of it, and nothing new is selected.
Editor::Editing::StepResult Step(bool undo, std::string& label);
} // namespace Editor::LiveMapEdit

#endif // _EDITOR
