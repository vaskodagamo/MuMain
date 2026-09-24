#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "Core/Globals/_types.h"  // vec3_t
#include "MapEditorFileUtil.h"      // SavedFile

class OBJECT;

// Map Editor object placement helpers.
//
// The client already loads every world object model (Data\Object{N}\Object{i}.bmd
// -> model type i-1) and can place them with CreateObject() and persist them with
// the engine's SaveObjects() (which encrypts). These helpers wrap that: they list
// which object types a world actually ships, place one on the ground, and save.
namespace Editor::ObjectPlace
{
    struct ModelEntry
    {
        int          type;  // CreateObject type (MODEL_WORLD_OBJECT + type)
        std::wstring file;  // e.g. "Object11.bmd"
    };

    // Lists the object models present in Data\Object{world}\ (files Object*.bmd),
    // sorted by type. These are the types safe to place (their .bmd is loaded).
    std::vector<ModelEntry> EnumerateModels(int world);

    // Where a click at world position (x,y) places an object: snapped to the tile
    // centre when `snap` is set, on the ground. Shared by placing and the placement
    // preview so they can never drift apart.
    void ComputePlacementPosition(float x, float y, bool snap, vec3_t outPos);

    // Saves all live objects back to Data/World{world}/EncTerrain{world}.obj
    // (encrypted by the engine SaveObjects) and copies the file into the
    // repository. Returns false on write failure. `outReport` gets the
    // status-line text with the absolute paths written; `outSaved`, when given,
    // where the file went.
    bool Save(int world, std::string& outReport, Editor::Files::SavedFile* outSaved = nullptr);

    // Returns the visible object under the mouse cursor (ray pick), or nullptr.
    OBJECT* PickUnderCursor();

    // Calls visit(o) for every live world object of the loaded map, block by block of
    // the object grid (the Outliner, the undo steps and "Objects follow terrain").
    void ForEachLiveObject(const std::function<void(OBJECT*)>& visit);

    // Every live object of `type` on the current map, ordered by position (by Y,
    // then X), so stepping through them visits them in the same order each time.
    std::vector<OBJECT*> LiveObjectsOfType(int type);

    // Sets counts[type] to the number of live objects of each type; `counts` is
    // resized to cover the highest type on the map. Reuses the vector's storage.
    void CountLiveObjects(std::vector<int>& counts);

    // The index of the record in the encrypted object file `objFile`
    // (EncTerrain{N}.obj) with this type at exactly this position, or -1 when
    // none matches (the object was moved or added after the file was saved).
    int FindRecordIndex(const std::filesystem::path& objFile, int type, const vec3_t position);

    // Moves `o` to (x,y,z). If the move crosses a 16x16 object-grid block the
    // object is re-created in the correct block (so live culling stays correct),
    // keeping its place in the saved file (OBJECT::SaveOrder); returns the
    // possibly-new object pointer.
    OBJECT* Reposition(OBJECT* o, float x, float y, float z);

    // Removes `o` from the world.
    void Remove(OBJECT* o);

    // The height of the ground under (x, y), the height a placed object gets.
    float GroundHeightAt(float x, float y);

    // Model `type`'s own name from its file, or "(type N)" when that is blank. A name that
    // is not UTF-8 (the Korean names of many of the game's models) comes back with each
    // byte outside ASCII as %XX (Editor::Text::ValidUtf8).
    std::string ModelName(int type);
}

#endif // _EDITOR
