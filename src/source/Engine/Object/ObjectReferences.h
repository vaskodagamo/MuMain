#pragma once

#include <unordered_set>

class OBJECT;

// Several engine arrays keep raw pointers to world objects (the objects in
// ObjectBlock): Operates[] lists the clickable objects such as Lorencia's chairs
// and pose boxes (read and written every frame by SelectOperate), and effects,
// joints and particles remember the object that spawned them and read it every
// frame. Freeing a world object without clearing those entries leaves them
// pointing at freed memory, so DeleteObject and DeleteAllObjects call these first.
namespace Engine::Object
{
// Removes every Operates entry of `object` (and the current operate selection if
// it was one of them) and ends the effects, joints and particles attached to it.
void ReleaseReferencesTo(const OBJECT* object);

// The same for a batch of objects about to be freed, in one pass over each array.
void ReleaseReferencesTo(const std::unordered_set<const OBJECT*>& objects);
} // namespace Engine::Object
