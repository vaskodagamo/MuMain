#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <string>
#include <utility>

namespace Editor::Editing
{
// One undoable Map Editor change, such as a moved group of objects or a brush
// stroke on the terrain. A command keeps what it needs to go both ways and the
// target it applies to. No ImGui, no engine: the targets are interfaces.
class EditCommand
{
public:
    explicit EditCommand(std::string label) : m_label(std::move(label)) {}
    virtual ~EditCommand() = default;
    EditCommand(const EditCommand&) = delete;
    EditCommand& operator=(const EditCommand&) = delete;

    // Puts the target back as it was before the change. False when the target no
    // longer matches the command (an object it names is missing).
    virtual bool Undo() = 0;
    // Makes the change again after an Undo, with the same result.
    virtual bool Redo() = 0;
    // The bytes the command keeps, for the history's memory limit.
    virtual std::size_t MemoryBytes() const = 0;

    // What the Undo/Redo buttons name, e.g. "Move 3 objects".
    const std::string& Label() const
    {
        return m_label;
    }

private:
    std::string m_label;
};
} // namespace Editor::Editing

#endif // _EDITOR
