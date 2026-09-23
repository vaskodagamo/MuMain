#include "stdafx.h"

#ifdef _EDITOR

#include "MapObjectWorld.h"

#include "MapObjectPlace.h"

#include "Engine/Object/ZzzObject.h"    // CreateObject
#include "Engine/Object/w_ObjectInfo.h" // class OBJECT

#include <algorithm>
#include <unordered_map>

using Editor::Editing::KeyedObjectState;
using Editor::Editing::ObjectState;
using Editor::ObjectPlace::ForEachLiveObject;

int CMapObjectWorld::FirstFreeKey()
{
    int highest = -1;
    ForEachLiveObject([&highest](const OBJECT* o) { highest = std::max(highest, o->SaveOrder); });
    return highest + 1;
}

int CMapObjectWorld::NextKey()
{
    if (m_nextKey < 0)
        m_nextKey = FirstFreeKey();
    return m_nextKey++;
}

int CMapObjectWorld::KeyOf(OBJECT* object)
{
    if (object->SaveOrder < 0)
        object->SaveOrder = NextKey();
    return object->SaveOrder;
}

OBJECT* CMapObjectWorld::Find(int key) const
{
    OBJECT* found = nullptr;
    ForEachLiveObject(
        [key, &found](OBJECT* o)
        {
            if (o->SaveOrder == key)
                found = o;
        });
    return found;
}

ObjectState CMapObjectWorld::StateOf(const OBJECT* object)
{
    ObjectState state;
    state.type = object->Type;
    VectorCopy(object->Position, state.position);
    VectorCopy(object->Angle, state.angle);
    state.scale = object->Scale;
    return state;
}

std::vector<KeyedObjectState> CMapObjectWorld::Capture(const std::vector<OBJECT*>& objects)
{
    std::vector<KeyedObjectState> states;
    states.reserve(objects.size());
    for (OBJECT* object : objects)
        states.push_back({KeyOf(object), StateOf(object)});
    return states;
}

std::vector<KeyedObjectState> CMapObjectWorld::CaptureAgain(const std::vector<KeyedObjectState>& earlier) const
{
    std::unordered_map<int, const OBJECT*> byKey;
    ForEachLiveObject([&byKey](const OBJECT* o) { byKey.emplace(o->SaveOrder, o); });
    std::vector<KeyedObjectState> states = earlier;
    for (KeyedObjectState& object : states)
    {
        const auto found = byKey.find(object.key);
        if (found != byKey.end())
            object.state = StateOf(found->second);
    }
    return states;
}

OBJECT* CMapObjectWorld::CreateWithKey(int key, const ObjectState& state)
{
    vec3_t position;
    vec3_t angle;
    VectorCopy(state.position, position);
    VectorCopy(state.angle, angle);
    OBJECT* created = CreateObject(state.type, position, angle, state.scale);
    if (created != nullptr)
        created->SaveOrder = key;
    return created;
}

OBJECT* CMapObjectWorld::CreateNew(const ObjectState& state)
{
    return CreateWithKey(NextKey(), state);
}

OBJECT* CMapObjectWorld::Apply(OBJECT* object, const ObjectState& state)
{
    OBJECT* moved = Editor::ObjectPlace::Reposition(object, state.position[0], state.position[1], state.position[2]);
    if (moved == nullptr)
        return nullptr;
    VectorCopy(state.angle, moved->Angle);
    moved->Scale = state.scale;
    return moved;
}

bool CMapObjectWorld::Create(int key, const ObjectState& state)
{
    m_touchedAny = true;
    if (CreateWithKey(key, state) == nullptr)
        return false;
    m_touchedKeys.push_back(key);
    return true;
}

bool CMapObjectWorld::Remove(int key)
{
    m_touchedAny = true;
    OBJECT* object = Find(key);
    if (object == nullptr)
        return false;
    Editor::ObjectPlace::Remove(object);
    return true;
}

bool CMapObjectWorld::Update(int key, const ObjectState& state)
{
    m_touchedAny = true;
    OBJECT* object = Find(key);
    if (object == nullptr)
        return false;
    Apply(object, state);
    m_touchedKeys.push_back(key);
    return true;
}

TouchedObjects CMapObjectWorld::TakeTouched()
{
    TouchedObjects touched;
    touched.any = m_touchedAny;
    if (!m_touchedKeys.empty())
    {
        std::unordered_map<int, OBJECT*> byKey;
        ForEachLiveObject([&byKey](OBJECT* o) { byKey.emplace(o->SaveOrder, o); });
        for (int key : m_touchedKeys)
        {
            const auto found = byKey.find(key);
            if (found != byKey.end())
                touched.live.push_back(found->second);
        }
    }
    m_touchedAny = false;
    m_touchedKeys.clear();
    return touched;
}

void CMapObjectWorld::Reset()
{
    // Taken now, while the loaded map has all its objects: a key an undo step still
    // names (a deleted object's) is never handed to a new object.
    m_nextKey = FirstFreeKey();
    m_touchedAny = false;
    m_touchedKeys.clear();
}

#endif // _EDITOR
