#pragma once

#ifdef _EDITOR

#include "EditCommand.h"
#include "ObjectWorld.h"

#include <optional>
#include <string>
#include <vector>

namespace Editor::Editing
{
// One object's part of an edit: no `before` means the edit created it, no `after`
// that the edit deleted it, both that it moved, turned or was scaled.
struct ObjectChange
{
    int key = 0;
    std::optional<ObjectState> before;
    std::optional<ObjectState> after;
};

// Changes for objects that were edited in place: `before` and `after` list the same
// keys in the same order. Objects whose state did not change are left out.
std::vector<ObjectChange> TransformChanges(const std::vector<KeyedObjectState>& before,
                                           const std::vector<KeyedObjectState>& after);
std::vector<ObjectChange> CreationChanges(const std::vector<KeyedObjectState>& created);
std::vector<ObjectChange> RemovalChanges(const std::vector<KeyedObjectState>& removed);

// Places, deletes, moves, turns or scales any number of world objects as one step.
// Undo walks the changes backwards, Redo forwards; neither re-creates an object
// that was not created or deleted by the edit itself.
class ObjectEditCommand : public EditCommand
{
public:
    ObjectEditCommand(std::string label, ObjectWorld& world, std::vector<ObjectChange> changes);

    bool Undo() override;
    bool Redo() override;
    std::size_t MemoryBytes() const override;

    const std::vector<ObjectChange>& Changes() const
    {
        return m_changes;
    }

private:
    bool ApplyChange(const ObjectChange& change, bool forward);

    ObjectWorld& m_world;
    std::vector<ObjectChange> m_changes;
};
} // namespace Editor::Editing

#endif // _EDITOR
