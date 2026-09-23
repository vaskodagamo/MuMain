#pragma once

#ifdef _EDITOR

#include "Editing/EditCommand.h"
#include "Editing/ObjectWorld.h"
#include "Editing/TerrainLayers.h"

#include <memory>
#include <vector>

class OBJECT;

// "Objects follow terrain" for the height brush: before a frame of a stroke writes
// the heights, BeforeBrush notes the ground under every object standing where the
// heights may change; AfterBrush moves each of them up or down by the change, so it
// keeps its height above the ground. Finish turns the stroke's moves into one undo
// step (joined to the stroke's own). That step applies through this class, which
// hands the moves to the history's object world without selecting the objects: an
// undo of a height stroke leaves the selection as it is.
class CMapGroundFollowers final : public Editor::Editing::ObjectWorld
{
public:
    // `heights` is the rectangle of height corners the brush may change this frame.
    void BeforeBrush(const Editor::Editing::CellRect& heights);
    void AfterBrush();
    // The moves of the stroke as one step (nullptr when nothing moved); the stroke ends.
    std::unique_ptr<Editor::Editing::EditCommand> Finish();
    // Ends the stroke without a step (its map was unloaded).
    void Cancel();

    bool Create(int key, const Editor::Editing::ObjectState& state) override;
    bool Remove(int key) override;
    bool Update(int key, const Editor::Editing::ObjectState& state) override;

private:
    struct Follower
    {
        OBJECT* object = nullptr;
        float groundBefore = 0.0f;
    };

    void Remember(OBJECT* object);

    std::vector<Follower> m_frame;                           // the objects of this frame
    std::vector<Editor::Editing::KeyedObjectState> m_before; // each moved object before the stroke
};

#endif // _EDITOR
