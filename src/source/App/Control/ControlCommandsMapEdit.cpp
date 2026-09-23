#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#ifdef _EDITOR

#include "App/Control/ControlMapArguments.h"
#include "Core/LiveMap.h"
#include "Core/LiveMapEdit.h"
#include "Core/LiveMapFiles.h"
#include "MapScript/ScriptParser.h"
#include "MapScript/ScriptReport.h"
#include "UI/MapEditor/MapEditorFileUtil.h" // ReadWholeFile

#include "json.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
using App::Control::Act;
using App::Control::EncodeError;
using App::Control::EncodeResult;
using App::Control::ErrorCode;
using App::Control::Request;
using Editor::MapInspect::SaveUnit;
using nlohmann::json;
namespace Arguments = App::Control::MapArguments;
namespace Script = Editor::MapScript;

constexpr const char* ALL_LAYERS = "all";
constexpr const char* HELD_EDIT = "a Map Editor stroke or drag is still held; release it and send the command again";
constexpr std::array<SaveUnit, Editor::MapInspect::SAVE_UNIT_COUNT> ALL_UNITS = {
    SaveUnit::Texture, SaveUnit::Height, SaveUnit::Attribute, SaveUnit::Light, SaveUnit::Objects};

// The steps the next map-undo and map-redo would apply (null when there is none).
void AddNextSteps(json& result, const Editor::LiveMapEdit::HistoryLabels& history)
{
    result["next_undo"] = history.undo.empty() ? json(nullptr) : json(history.undo.back());
    result["next_redo"] = history.redo.empty() ? json(nullptr) : json(history.redo.front());
}

// The script's JSON text: `script` (an object, or the object as a string) or the file
// `path` names.
bool ScriptText(const Request& request, std::string& text, std::string& error)
{
    if (request.Has("script") == request.Has("path"))
    {
        error = "give `script` (the edit script object) or `path` (a JSON file holding it), one of them";
        return false;
    }
    if (request.Has("script"))
    {
        if (request.GetStructured("script", text) || request.GetString("script", text))
            return true;
        error = "`script` is the edit script: {\"schema\": \"mu-map-edit/1\", \"ops\": [...]}";
        return false;
    }
    std::string path;
    if (!request.GetString("path", path) || path.empty())
    {
        error = "`path` is the path of a JSON file holding the edit script";
        return false;
    }
    const std::filesystem::path file = Arguments::ResolvePath(path);
    const std::vector<unsigned char> bytes = Editor::Files::ReadWholeFile(file);
    if (bytes.empty())
    {
        error = "cannot read " + Arguments::PathJson(file);
        return false;
    }
    text.assign(bytes.begin(), bytes.end());
    return true;
}

bool UnitFromName(const std::string& name, SaveUnit& unit)
{
    for (SaveUnit candidate : ALL_UNITS)
    {
        if (name == Editor::MapInspect::SaveUnitName(candidate))
        {
            unit = candidate;
            return true;
        }
    }
    return false;
}

std::string UnitNamesHelp()
{
    return "`layers` is \"all\", one of the map's files or a list of them: texture, height, attribute, light, "
           "objects";
}

// `layers` as one name ("all" for every file).
bool ReadUnitName(const std::string& name, std::vector<SaveUnit>& units, std::string& error)
{
    SaveUnit unit = SaveUnit::Texture;
    if (name == ALL_LAYERS)
        units.assign(ALL_UNITS.begin(), ALL_UNITS.end());
    else if (UnitFromName(name, unit))
        units.push_back(unit);
    else
        error = "unknown layer \"" + name + "\"; " + UnitNamesHelp();
    return error.empty();
}

// `layers`: "all", one of the map's files by its unit name, or a list of them.
bool ReadUnits(const Request& request, std::vector<SaveUnit>& units, std::string& error)
{
    std::string name;
    if (request.GetString("layers", name))
        return ReadUnitName(name, units, error);
    std::string encoded;
    const json names = request.GetStructured("layers", encoded) ? json::parse(encoded, nullptr, false) : json();
    if (!names.is_array() || names.empty())
    {
        error = UnitNamesHelp();
        return false;
    }
    for (const json& entry : names)
    {
        SaveUnit unit = SaveUnit::Texture;
        if (!entry.is_string() || !UnitFromName(entry.get<std::string>(), unit))
        {
            error = "unknown layer " + entry.dump() + "; " + UnitNamesHelp();
            return false;
        }
        if (std::find(units.begin(), units.end(), unit) == units.end())
            units.push_back(unit);
    }
    return true;
}

std::vector<SaveUnit> UnsavedUnits()
{
    const auto unsaved = Editor::LiveMap::UnsavedUnits();
    std::vector<SaveUnit> units;
    for (SaveUnit unit : ALL_UNITS)
    {
        if (unsaved[static_cast<std::size_t>(unit)])
            units.push_back(unit);
    }
    return units;
}

json SavedJson(const Editor::LiveMapFiles::SavedUnit& saved)
{
    json entry;
    entry["layer"] = std::string(Editor::MapInspect::SaveUnitName(saved.unit));
    entry["saved"] = saved.saved;
    entry["report"] = saved.report;
    if (saved.saved)
        entry.update(Arguments::SavedFileJson(saved.file));
    return entry;
}

json UnitNames(const std::vector<SaveUnit>& units)
{
    json names = json::array();
    for (SaveUnit unit : units)
        names.push_back(std::string(Editor::MapInspect::SaveUnitName(unit)));
    return names;
}

std::string HistoryStep(const Request& request, bool undo)
{
    if (Editor::LiveMapEdit::IsEditHeld())
        return EncodeError(request.EncodedId(), ErrorCode::Busy, HELD_EDIT);
    std::string label;
    const Editor::Editing::StepResult result = Editor::LiveMapEdit::Step(undo, label);
    if (result == Editor::Editing::StepResult::Nothing)
        return EncodeError(request.EncodedId(), ErrorCode::Failed,
                           undo ? "there is nothing to undo" : "there is nothing to redo");
    if (result == Editor::Editing::StepResult::Failed)
        return EncodeError(request.EncodedId(), ErrorCode::Failed,
                           "the step \"" + label +
                               "\" no longer matched the map (an object it names is gone); "
                               "the undo history was cleared");
    const Editor::LiveMapEdit::HistoryLabels history = Editor::LiveMapEdit::History();
    json answer;
    answer[undo ? "undone" : "redone"] = label;
    AddNextSteps(answer, history);
    answer["unsaved"] = Arguments::UnsavedJson();
    return EncodeResult(request.EncodedId(), answer.dump());
}
} // namespace

namespace App::Control::Commands
{
std::string MapApply(const Request& request, std::unique_ptr<Act>&)
{
    std::string text;
    std::string error;
    bool dryRun = false;
    Script::EditScript script;
    if (!Arguments::ReadDryRun(request, dryRun, error) || !ScriptText(request, text, error) ||
        !Script::ParseScript(text, script, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);
    if (!dryRun && Editor::LiveMapEdit::IsEditHeld())
        return EncodeError(request.EncodedId(), ErrorCode::Busy, HELD_EDIT);

    Editor::LiveMapEdit::ApplyResult applied;
    if (!Editor::LiveMapEdit::Apply(script, dryRun, applied, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);
    json result = Script::ReportJson(applied.reports, applied.changes, applied.context);
    result["label"] = applied.label;
    result["dry_run"] = dryRun;
    result["changed"] = applied.changes.Any();
    result["applied"] = applied.applied;
    if (!dryRun)
        result["unsaved"] = Arguments::UnsavedJson();
    return EncodeResult(request.EncodedId(), result.dump());
}

std::string MapUndo(const Request& request, std::unique_ptr<Act>&)
{
    return HistoryStep(request, true);
}

std::string MapRedo(const Request& request, std::unique_ptr<Act>&)
{
    return HistoryStep(request, false);
}

std::string MapHistory(const Request& request, std::unique_ptr<Act>&)
{
    const Editor::LiveMapEdit::HistoryLabels history = Editor::LiveMapEdit::History();
    json result;
    result["undo"] = history.undo;
    result["redo"] = history.redo;
    AddNextSteps(result, history);
    result["memory_bytes"] = history.memoryBytes;
    return EncodeResult(request.EncodedId(), result.dump());
}

std::string MapSave(const Request& request, std::unique_ptr<Act>&)
{
    std::vector<SaveUnit> units;
    std::string error;
    if (request.Has("layers") && !ReadUnits(request, units, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);
    if (!request.Has("layers"))
        units = UnsavedUnits();
    if (Editor::LiveMapEdit::IsEditHeld())
        return EncodeError(request.EncodedId(), ErrorCode::Busy, HELD_EDIT);

    json saved = json::array();
    std::vector<std::string> failed;
    for (const Editor::LiveMapFiles::SavedUnit& unit : Editor::LiveMapFiles::Save(units))
    {
        saved.push_back(SavedJson(unit));
        if (!unit.saved)
            failed.push_back(std::string(Editor::MapInspect::SaveUnitName(unit.unit)));
    }
    json result;
    result["world"] = Editor::LiveMap::WorldFolder();
    result["saved"] = std::move(saved);
    result["unsaved"] = Arguments::UnsavedJson();
    if (failed.empty())
        return EncodeResult(request.EncodedId(), result.dump());
    std::string names;
    for (const std::string& name : failed)
        names += (names.empty() ? "" : ", ") + name;
    return EncodeError(request.EncodedId(), ErrorCode::Failed, "could not save " + names + " (see each `report`)",
                       result.dump());
}

std::string MapRevert(const Request& request, std::unique_ptr<Act>&)
{
    std::vector<SaveUnit> units;
    std::string error;
    if (!request.Has("layers") || !ReadUnits(request, units, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           error.empty() ? "map-revert needs `layers`; " + UnitNamesHelp() : error);
    if (Editor::LiveMapEdit::IsEditHeld())
        return EncodeError(request.EncodedId(), ErrorCode::Busy, HELD_EDIT);
    if (!Editor::LiveMapFiles::Revert(units, error))
        return EncodeError(request.EncodedId(), ErrorCode::Failed, error);
    json result;
    result["reverted"] = UnitNames(units);
    result["history_cleared"] = true;
    result["unsaved"] = Arguments::UnsavedJson();
    return EncodeResult(request.EncodedId(), result.dump());
}
} // namespace App::Control::Commands

#endif // _EDITOR
