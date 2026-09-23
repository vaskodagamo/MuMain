#pragma once

#ifdef _EDITOR

#include "EditCommand.h"

#include <memory>
#include <string>
#include <vector>

namespace Editor::Editing
{
// Several commands made by one user action, undone and redone as one step: a height
// stroke with the objects that followed the ground. Undo runs the parts newest
// first, Redo oldest first.
class EditCommandGroup : public EditCommand
{
public:
    // Parts that are null (an edit that changed nothing) are left out.
    EditCommandGroup(std::string label, std::vector<std::unique_ptr<EditCommand>> parts);

    bool Undo() override;
    bool Redo() override;
    std::size_t MemoryBytes() const override;

    bool IsEmpty() const
    {
        return m_parts.empty();
    }

private:
    std::vector<std::unique_ptr<EditCommand>> m_parts;
};

// The group of `parts`; the only part itself when there is one (keeping its own
// label), nullptr when none changed anything.
std::unique_ptr<EditCommand> GroupEdits(std::string label, std::vector<std::unique_ptr<EditCommand>> parts);
} // namespace Editor::Editing

#endif // _EDITOR
