#pragma once

#ifdef _EDITOR

#include "EditCommand.h"

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace Editor::Editing
{
// The history keeps commands up to this many bytes, then drops the oldest ones.
// A terrain stroke over the whole 256x256 map keeps about 1 MB, a typical brush
// stroke a few KB and an object edit a few hundred bytes.
constexpr std::size_t UNDO_MEMORY_LIMIT_BYTES = 64u * 1024u * 1024u;

// What an Undo or Redo did.
enum class StepResult
{
    Nothing, // there was nothing to undo or redo
    Done,
    Failed, // the command no longer matched the map; the history was cleared
};

// The Map Editor's undo history, shared by all editing tabs. Pushing a command
// drops everything that could be redone. When the commands keep more than the
// memory limit, the oldest ones are dropped; the newest always stays.
class CommandStack
{
public:
    explicit CommandStack(std::size_t memoryLimitBytes = UNDO_MEMORY_LIMIT_BYTES);

    // Takes a command whose change was already made. A null command is ignored.
    void Push(std::unique_ptr<EditCommand> command);
    StepResult Undo();
    StepResult Redo();
    void Clear();

    bool CanUndo() const;
    bool CanRedo() const;
    // Label of the command the next Undo / Redo applies; empty when there is none.
    const std::string& UndoLabel() const;
    const std::string& RedoLabel() const;

    std::size_t UndoCount() const;
    std::size_t RedoCount() const;
    std::size_t MemoryBytes() const;

private:
    void DropOldestOverLimit();

    std::size_t m_memoryLimit;
    std::size_t m_memoryBytes = 0;
    std::deque<std::unique_ptr<EditCommand>> m_undo;  // oldest first
    std::vector<std::unique_ptr<EditCommand>> m_redo; // next redo last
};
} // namespace Editor::Editing

#endif // _EDITOR
