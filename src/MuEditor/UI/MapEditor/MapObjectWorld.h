#pragma once

#ifdef _EDITOR

#include "Editing/ObjectWorld.h"

#include <vector>

class OBJECT;

// What an undo or redo did to the world objects.
struct TouchedObjects
{
    bool any = false;          // an object command was applied
    std::vector<OBJECT*> live; // the objects it created or changed, as they are now
};

// The loaded map's world objects as the Map Editor's undo history sees them. An
// object is named by its OBJECT::SaveOrder, which an undo gives back to an object
// it re-creates. Creating, deleting and moving go through Editor::ObjectPlace, so
// a deleted object's Operates[] entry and attached effects are dropped first and a
// move across an object-grid block re-creates the object in the right block.
class CMapObjectWorld final : public Editor::Editing::ObjectWorld
{
public:
    // The object's key; an object the game made at run time gets the next free one.
    int KeyOf(OBJECT* object);
    // The live object with `key`, or nullptr.
    OBJECT* Find(int key) const;
    static Editor::Editing::ObjectState StateOf(const OBJECT* object);
    std::vector<Editor::Editing::KeyedObjectState> Capture(const std::vector<OBJECT*>& objects);
    // The objects of `earlier` as they are now, in the same order; an object that is
    // gone keeps its earlier state (so it shows no change).
    std::vector<Editor::Editing::KeyedObjectState>
    CaptureAgain(const std::vector<Editor::Editing::KeyedObjectState>& earlier) const;

    // Places a new object with a new key; nullptr outside the object grid.
    OBJECT* CreateNew(const Editor::Editing::ObjectState& state);
    // Moves, turns and scales `object`. Returns it, or the object that replaced it
    // when the move crossed an object-grid block (nullptr if that failed).
    OBJECT* Apply(OBJECT* object, const Editor::Editing::ObjectState& state);

    bool Create(int key, const Editor::Editing::ObjectState& state) override;
    bool Remove(int key) override;
    bool Update(int key, const Editor::Editing::ObjectState& state) override;

    // What the undo or redo since the last call did (see TouchedObjects).
    TouchedObjects TakeTouched();
    // The map's objects were freed and the next map's loaded: new keys start after the
    // highest one they have.
    void Reset();

private:
    OBJECT* CreateWithKey(int key, const Editor::Editing::ObjectState& state);
    static int FirstFreeKey();
    int NextKey();

    int m_nextKey = -1; // -1: not known yet (no map change seen), found from the live objects when needed
    bool m_touchedAny = false;
    std::vector<int> m_touchedKeys;
};

#endif // _EDITOR
