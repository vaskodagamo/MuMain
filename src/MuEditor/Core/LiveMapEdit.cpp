#include "stdafx.h"

#ifdef _EDITOR

#include "LiveMapEdit.h"

#include "LiveMap.h"
#include "MapScript/ScriptRunner.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "UI/MapEditor/MapEditHistory.h"
#include "UI/MapEditor/MapEditorUI.h"
#include "UI/MapEditor/MapObjectEditor.h"
#include "UI/MapEditor/MapObjectPlace.h"
#include "UI/MapEditor/MapTerrainLayers.h"

#include "Editing/EditCommandGroup.h"
#include "Editing/ObjectEditCommand.h"
#include "Editing/TerrainPatchCommand.h" // ReadRect, WriteRect
#include "Editing/TerrainStroke.h"
#include "Engine/Object/WorldObjectFile.h"
#include "Engine/Object/ZzzObject.h"      // IsSavedWorldObject
#include "Engine/Object/w_ObjectInfo.h"   // class OBJECT
#include "Render/Terrain/ZzzLodTerrain.h" // IsTerrainHeightExtMap
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>

extern float g_fSpecialHeight; // RequestTerrainHeight's height on special-height tiles

namespace Editor::LiveMapEdit
{
namespace
{
namespace Script = Editor::MapScript;
using Editor::Editing::CellRect;
using Editor::Editing::EditCommand;
using Editor::Editing::ObjectChange;

constexpr int TILES = Editor::MapInspect::MAP_TILES;

std::vector<OBJECT*> SavedObjects()
{
    std::vector<OBJECT*> objects;
    Editor::ObjectPlace::ForEachLiveObject(
        [&objects](OBJECT* object)
        {
            if (IsSavedWorldObject(object))
                objects.push_back(object);
        });
    return objects;
}

// The saved objects in save order (as map-query numbers them), each with its key.
std::vector<Script::MapObject> LiveObjects()
{
    CMapObjectWorld& world = g_MapEditHistory.Objects();
    const std::vector<OBJECT*> objects = SavedObjects();
    std::vector<int> keys;
    keys.reserve(objects.size());
    for (OBJECT* object : objects)
        keys.push_back(world.KeyOf(object));
    std::vector<Script::MapObject> ordered;
    for (std::size_t index : Engine::Object::WorldObjectFile::SaveOrderIndices(keys))
    {
        Script::MapObject object;
        object.key = keys[index];
        object.id = static_cast<int>(ordered.size());
        object.state = CMapObjectWorld::StateOf(objects[index]);
        ordered.push_back(object);
    }
    return ordered;
}

void AddModel(std::vector<Script::ModelInfo>& models, int type)
{
    const bool known = std::any_of(models.begin(), models.end(),
                                   [type](const Script::ModelInfo& model) { return model.type == type; });
    if (!known)
        models.push_back(
            Script::ModelInfo{type, Editor::ObjectPlace::ModelName(type), Editor::LiveMap::CatalogName(type)});
}

// The models the map can place, then those of `objectTypes` that are not among them.
std::vector<Script::ModelInfo> ModelsWith(const std::vector<int>& objectTypes)
{
    std::vector<Script::ModelInfo> models;
    for (const Editor::ObjectPlace::ModelEntry& entry : Editor::ObjectPlace::EnumerateModels(LiveMap::WorldFolder()))
        AddModel(models, entry.type);
    // Objects the map's file placed with a model above the placeable range are loaded too.
    for (const int type : objectTypes)
        AddModel(models, type);
    return models;
}

Script::MapContext LiveContext(const Script::MapState& state)
{
    Script::MapContext context;
    context.mapIndex = gMapManager.WorldActive;
    std::vector<int> objectTypes;
    objectTypes.reserve(state.objects.size());
    for (const Script::MapObject& object : state.objects)
        objectTypes.push_back(object.state.type);
    context.models = ModelsWith(objectTypes);
    context.tileSlots = Editor::LiveMap::TileSlots();
    context.maxHeight = Editor::LiveMap::MaxStoredHeight();
    context.specialHeight = g_fSpecialHeight;
    context.heightsEditable = !IsTerrainHeightExtMap(gMapManager.WorldActive);
    context.maxObjects = Engine::Object::WorldObjectFile::MAX_RECORDS;
    return context;
}

// The edited copy's bytes of a live layer, laid out as the live array.
const std::uint8_t* EditedLayer(const Script::MapTerrain& edited, int layer)
{
    switch (layer)
    {
    case MAP_LAYER_TILE_BASE:
        return edited.baseTiles.data();
    case MAP_LAYER_TILE_OVERLAY:
        return edited.overlayTiles.data();
    case MAP_LAYER_TILE_ALPHA:
        return reinterpret_cast<const std::uint8_t*>(edited.overlayAlpha.data());
    case MAP_LAYER_HEIGHT:
        return reinterpret_cast<const std::uint8_t*>(edited.height.data());
    case MAP_LAYER_WALL:
        return reinterpret_cast<const std::uint8_t*>(edited.attribute.data());
    case MAP_LAYER_LIGHT:
        return reinterpret_cast<const std::uint8_t*>(edited.light.data());
    default:
        return nullptr;
    }
}

// The live layers a script changed, each with the rectangle to copy.
std::vector<std::pair<int, CellRect>> ChangedLayers(const Script::MapChanges& changes)
{
    std::vector<std::pair<int, CellRect>> layers;
    if (changes.texture1.Changed())
        layers.emplace_back(MAP_LAYER_TILE_BASE, changes.texture1.area);
    if (changes.texture2.Changed())
    {
        layers.emplace_back(MAP_LAYER_TILE_OVERLAY, changes.texture2.area);
        layers.emplace_back(MAP_LAYER_TILE_ALPHA, changes.texture2.area);
    }
    if (changes.height.Changed())
        layers.emplace_back(MAP_LAYER_HEIGHT, changes.height.area);
    if (changes.attribute.Changed())
        layers.emplace_back(MAP_LAYER_WALL, changes.attribute.area);
    if (changes.light.Changed())
        layers.emplace_back(MAP_LAYER_LIGHT, changes.light.area);
    return layers;
}

// Copies the changed rectangles into the live arrays as one terrain stroke, relighting
// what the heights and the light map need, and returns its undo part.
std::unique_ptr<EditCommand> CommitTerrain(const Script::MapTerrain& edited, const Script::MapChanges& changes,
                                           const std::string& label)
{
    const std::vector<std::pair<int, CellRect>> layers = ChangedLayers(changes);
    if (layers.empty())
        return nullptr;
    CMapTerrainLayers& live = g_MapEditHistory.Terrain();
    std::vector<int> layerIds;
    for (const auto& [layer, rect] : layers)
        layerIds.push_back(layer);
    Editor::Editing::TerrainStroke stroke;
    stroke.Begin(live, layerIds, label);
    for (const auto& [layer, rect] : layers)
    {
        const std::vector<std::uint8_t> bytes =
            Editor::Editing::ReadRect(EditedLayer(edited, layer), TILES, live.ElementBytes(layer), rect);
        Editor::Editing::WriteRect(live, layer, rect, bytes);
    }
    if (changes.height.Changed())
        CMapTerrainLayers::RelightHeights(changes.height.area);
    if (changes.light.Changed())
        CMapTerrainLayers::RelightCells(changes.light.area);
    return stroke.Finish();
}

// Makes the object changes on the live map and returns their undo part.
std::unique_ptr<EditCommand> CommitObjects(const Script::MapChanges& changes, const std::string& label)
{
    if (changes.objects.empty())
        return nullptr;
    CMapObjectWorld& world = g_MapEditHistory.Objects();
    std::unordered_map<int, OBJECT*> byKey;
    Editor::ObjectPlace::ForEachLiveObject([&byKey](OBJECT* object) { byKey.emplace(object->SaveOrder, object); });
    std::vector<ObjectChange> made;
    for (const Script::ObjectDiff& diff : changes.objects)
    {
        const auto found = byKey.find(diff.key);
        OBJECT* object = found != byKey.end() ? found->second : nullptr;
        if (diff.kind == Script::ObjectChangeKind::Added)
        {
            OBJECT* created = world.CreateNew(*diff.after);
            if (created != nullptr)
                made.push_back({created->SaveOrder, std::nullopt, diff.after});
            continue;
        }
        if (object == nullptr)
            continue;
        if (diff.kind == Script::ObjectChangeKind::Removed)
        {
            Editor::ObjectPlace::Remove(object);
            made.push_back({diff.key, diff.before, std::nullopt});
            continue;
        }
        if (world.Apply(object, *diff.after) != nullptr)
            made.push_back({diff.key, diff.before, diff.after});
    }
    if (made.size() != changes.objects.size())
        g_MuEditorConsoleUI.LogEditor("[MapEditor] map-apply: " + std::to_string(changes.objects.size() - made.size()) +
                                      " object change(s) could not be made on the live map");
    return std::make_unique<Editor::Editing::ObjectEditCommand>(label, world, std::move(made));
}

// The Map Editor's selection by key across a scripted step, which may delete or
// re-create the objects it points at.
class KeptSelection
{
public:
    KeptSelection()
    {
        for (OBJECT* object : g_MapObjectEditor.Selection().Objects())
            m_keys.push_back(g_MapEditHistory.Objects().KeyOf(object));
    }

    void Restore() const
    {
        CMapObjectWorld& world = g_MapEditHistory.Objects();
        world.TakeTouched(); // a scripted step selects nothing
        std::vector<OBJECT*> objects;
        for (int key : m_keys)
        {
            if (OBJECT* object = world.Find(key))
                objects.push_back(object);
        }
        g_MapObjectEditor.Selection().Assign(objects);
    }

private:
    std::vector<int> m_keys;
};

void Commit(const Script::MapState& edited, const ApplyResult& result)
{
    const KeptSelection selection;
    g_MapEditorUI.BeforeScriptedEdit();
    std::vector<std::unique_ptr<EditCommand>> parts;
    parts.push_back(CommitTerrain(edited.terrain, result.changes, result.label));
    parts.push_back(CommitObjects(result.changes, result.label));
    g_MapEditHistory.Push(Editor::Editing::GroupEdits(result.label, std::move(parts)));
    if (result.changes.attribute.Changed())
        g_MapEditorUI.NoteScriptedWallEdits(result.changes.attribute.area);
    selection.Restore();
    g_MuEditorConsoleUI.LogEditor("[MapEditor] map-apply: " + result.label);
}
} // namespace

Snapshot TakeSnapshot()
{
    Snapshot snapshot;
    snapshot.state.terrain = Script::MapTerrain::CopyOf(Editor::LiveMap::Terrain());
    snapshot.state.objects = LiveObjects();
    snapshot.state.nextId = static_cast<int>(snapshot.state.objects.size());
    snapshot.context = LiveContext(snapshot.state);
    return snapshot;
}

std::vector<Script::ModelInfo> LoadedModels()
{
    std::vector<int> objectTypes;
    for (const Editor::MapInspect::MapObjectRecord& object : Editor::LiveMap::Objects())
        objectTypes.push_back(object.type);
    return ModelsWith(objectTypes);
}

bool Apply(const Script::EditScript& script, bool dryRun, ApplyResult& result, std::string& error)
{
    g_MapEditorUI.ForgetUnloadedMap();
    const Snapshot snapshot = TakeSnapshot();
    Script::MapState edited = snapshot.state;
    if (!Script::RunScript(script, snapshot.context, edited, result.reports, error))
        return false;
    result.label = Script::StepLabel(script);
    result.changes = Script::Diff(snapshot.state, edited);
    result.context = snapshot.context;
    if (dryRun || !result.changes.Any())
        return true;
    Commit(edited, result);
    result.applied = true;
    return true;
}

bool IsEditHeld()
{
    return g_MapEditorUI.IsEditInProgress();
}

HistoryLabels History()
{
    g_MapEditorUI.ForgetUnloadedMap();
    HistoryLabels labels;
    labels.undo = g_MapEditHistory.UndoLabels();
    labels.redo = g_MapEditHistory.RedoLabels();
    labels.memoryBytes = g_MapEditHistory.MemoryBytes();
    return labels;
}

Editor::Editing::StepResult Step(bool undo, std::string& label)
{
    g_MapEditorUI.ForgetUnloadedMap();
    label = undo ? g_MapEditHistory.UndoLabel() : g_MapEditHistory.RedoLabel();
    const KeptSelection selection;
    const Editor::Editing::StepResult result = undo ? g_MapEditHistory.UndoStep() : g_MapEditHistory.RedoStep();
    selection.Restore();
    return result;
}
} // namespace Editor::LiveMapEdit

#endif // _EDITOR
