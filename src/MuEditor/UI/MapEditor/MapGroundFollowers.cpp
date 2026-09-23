#include "stdafx.h"

#ifdef _EDITOR

#include "MapGroundFollowers.h"

#include "MapEditHistory.h"
#include "MapObjectPlace.h"

#include "Editing/ObjectEditCommand.h"
#include "Engine/Object/w_ObjectInfo.h" // class OBJECT

#include <algorithm>
#include <cmath>

using Editor::Editing::CellRect;
using Editor::Editing::KeyedObjectState;
using Editor::Editing::ObjectState;

namespace
{
// The ground under an object is interpolated from the four corners of its tile, so an
// object on tile (x, y) reads the heights of corners x..x+1, y..y+1.
bool StandsOnChangedGround(const OBJECT* object, const CellRect& heights)
{
    const auto tileX = static_cast<int>(std::floor(object->Position[0] / TERRAIN_SCALE));
    const auto tileY = static_cast<int>(std::floor(object->Position[1] / TERRAIN_SCALE));
    return tileX + 1 >= heights.minX && tileX <= heights.maxX && tileY + 1 >= heights.minY && tileY <= heights.maxY;
}
} // namespace

void CMapGroundFollowers::BeforeBrush(const CellRect& heights)
{
    m_frame.clear();
    if (heights.IsEmpty())
        return;
    // Checking every object (a few thousand per map) takes microseconds and does not
    // depend on how CreateObject numbers the object-grid blocks.
    Editor::ObjectPlace::ForEachLiveObject(
        [this, &heights](OBJECT* o)
        {
            if (StandsOnChangedGround(o, heights))
                m_frame.push_back({o, Editor::ObjectPlace::GroundHeightAt(o->Position[0], o->Position[1])});
        });
}

void CMapGroundFollowers::Remember(OBJECT* object)
{
    CMapObjectWorld& world = g_MapEditHistory.Objects();
    const int key = world.KeyOf(object);
    const bool known = std::any_of(m_before.begin(), m_before.end(),
                                   [key](const KeyedObjectState& state) { return state.key == key; });
    if (!known)
        m_before.push_back({key, CMapObjectWorld::StateOf(object)});
}

void CMapGroundFollowers::AfterBrush()
{
    for (const Follower& follower : m_frame)
    {
        OBJECT* object = follower.object;
        const float change =
            Editor::ObjectPlace::GroundHeightAt(object->Position[0], object->Position[1]) - follower.groundBefore;
        if (change == 0.0f)
            continue;
        Remember(object);
        ObjectState state = CMapObjectWorld::StateOf(object);
        state.position[2] += change;
        g_MapEditHistory.Objects().Apply(object, state);
    }
    m_frame.clear();
}

std::unique_ptr<Editor::Editing::EditCommand> CMapGroundFollowers::Finish()
{
    m_frame.clear();
    if (m_before.empty())
        return nullptr;
    const std::vector<KeyedObjectState> after = g_MapEditHistory.Objects().CaptureAgain(m_before);
    std::vector<Editor::Editing::ObjectChange> changes = Editor::Editing::TransformChanges(m_before, after);
    m_before.clear();
    if (changes.empty())
        return nullptr;
    return std::make_unique<Editor::Editing::ObjectEditCommand>("Objects follow ground", *this, std::move(changes));
}

void CMapGroundFollowers::Cancel()
{
    m_frame.clear();
    m_before.clear();
}

bool CMapGroundFollowers::Create(int key, const ObjectState& state)
{
    const bool ok = g_MapEditHistory.Objects().Create(key, state);
    g_MapEditHistory.Objects().TakeTouched(); // not an object edit: keep the selection
    return ok;
}

bool CMapGroundFollowers::Remove(int key)
{
    const bool ok = g_MapEditHistory.Objects().Remove(key);
    g_MapEditHistory.Objects().TakeTouched();
    return ok;
}

bool CMapGroundFollowers::Update(int key, const ObjectState& state)
{
    const bool ok = g_MapEditHistory.Objects().Update(key, state);
    g_MapEditHistory.Objects().TakeTouched();
    return ok;
}

#endif // _EDITOR
