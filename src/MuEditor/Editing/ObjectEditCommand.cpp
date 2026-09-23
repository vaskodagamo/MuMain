#include "ObjectEditCommand.h"

#ifdef _EDITOR

#include <utility>

namespace Editor::Editing
{
std::vector<ObjectChange> TransformChanges(const std::vector<KeyedObjectState>& before,
                                           const std::vector<KeyedObjectState>& after)
{
    std::vector<ObjectChange> changes;
    const std::size_t count = before.size() < after.size() ? before.size() : after.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        if (before[i].key != after[i].key || before[i].state == after[i].state)
            continue;
        changes.push_back({before[i].key, before[i].state, after[i].state});
    }
    return changes;
}

std::vector<ObjectChange> CreationChanges(const std::vector<KeyedObjectState>& created)
{
    std::vector<ObjectChange> changes;
    changes.reserve(created.size());
    for (const KeyedObjectState& object : created)
        changes.push_back({object.key, std::nullopt, object.state});
    return changes;
}

std::vector<ObjectChange> RemovalChanges(const std::vector<KeyedObjectState>& removed)
{
    std::vector<ObjectChange> changes;
    changes.reserve(removed.size());
    for (const KeyedObjectState& object : removed)
        changes.push_back({object.key, object.state, std::nullopt});
    return changes;
}

ObjectEditCommand::ObjectEditCommand(std::string label, ObjectWorld& world, std::vector<ObjectChange> changes)
    : EditCommand(std::move(label)), m_world(world), m_changes(std::move(changes))
{
}

bool ObjectEditCommand::ApplyChange(const ObjectChange& change, bool forward)
{
    const std::optional<ObjectState>& from = forward ? change.before : change.after;
    const std::optional<ObjectState>& to = forward ? change.after : change.before;
    if (from && to)
        return m_world.Update(change.key, *to);
    if (to)
        return m_world.Create(change.key, *to);
    if (from)
        return m_world.Remove(change.key);
    return true;
}

bool ObjectEditCommand::Undo()
{
    bool allApplied = true;
    for (auto it = m_changes.rbegin(); it != m_changes.rend(); ++it)
        allApplied = ApplyChange(*it, false) && allApplied;
    return allApplied;
}

bool ObjectEditCommand::Redo()
{
    bool allApplied = true;
    for (const ObjectChange& change : m_changes)
        allApplied = ApplyChange(change, true) && allApplied;
    return allApplied;
}

std::size_t ObjectEditCommand::MemoryBytes() const
{
    return sizeof(*this) + Label().capacity() + m_changes.capacity() * sizeof(ObjectChange);
}
} // namespace Editor::Editing

#endif // _EDITOR
