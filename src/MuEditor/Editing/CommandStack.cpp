#include "CommandStack.h"

#ifdef _EDITOR

#include <utility>

namespace Editor::Editing
{
namespace
{
const std::string NO_LABEL;
} // namespace

CommandStack::CommandStack(std::size_t memoryLimitBytes) : m_memoryLimit(memoryLimitBytes) {}

void CommandStack::Push(std::unique_ptr<EditCommand> command)
{
    if (!command)
        return;
    for (const std::unique_ptr<EditCommand>& redo : m_redo)
        m_memoryBytes -= redo->MemoryBytes();
    m_redo.clear();

    m_memoryBytes += command->MemoryBytes();
    m_undo.push_back(std::move(command));
    DropOldestOverLimit();
}

void CommandStack::DropOldestOverLimit()
{
    while (m_memoryBytes > m_memoryLimit && m_undo.size() > 1)
    {
        m_memoryBytes -= m_undo.front()->MemoryBytes();
        m_undo.pop_front();
    }
}

StepResult CommandStack::Undo()
{
    if (m_undo.empty())
        return StepResult::Nothing;
    std::unique_ptr<EditCommand> command = std::move(m_undo.back());
    m_undo.pop_back();
    if (!command->Undo())
    {
        Clear();
        return StepResult::Failed;
    }
    m_redo.push_back(std::move(command));
    return StepResult::Done;
}

StepResult CommandStack::Redo()
{
    if (m_redo.empty())
        return StepResult::Nothing;
    std::unique_ptr<EditCommand> command = std::move(m_redo.back());
    m_redo.pop_back();
    if (!command->Redo())
    {
        Clear();
        return StepResult::Failed;
    }
    m_undo.push_back(std::move(command));
    return StepResult::Done;
}

void CommandStack::Clear()
{
    m_undo.clear();
    m_redo.clear();
    m_memoryBytes = 0;
}

bool CommandStack::CanUndo() const
{
    return !m_undo.empty();
}

bool CommandStack::CanRedo() const
{
    return !m_redo.empty();
}

const std::string& CommandStack::UndoLabel() const
{
    return m_undo.empty() ? NO_LABEL : m_undo.back()->Label();
}

const std::string& CommandStack::RedoLabel() const
{
    return m_redo.empty() ? NO_LABEL : m_redo.back()->Label();
}

std::vector<std::string> CommandStack::UndoLabels() const
{
    std::vector<std::string> labels;
    labels.reserve(m_undo.size());
    for (const std::unique_ptr<EditCommand>& command : m_undo)
        labels.push_back(command->Label());
    return labels;
}

std::vector<std::string> CommandStack::RedoLabels() const
{
    std::vector<std::string> labels;
    labels.reserve(m_redo.size());
    for (auto command = m_redo.rbegin(); command != m_redo.rend(); ++command)
        labels.push_back((*command)->Label());
    return labels;
}

std::size_t CommandStack::UndoCount() const
{
    return m_undo.size();
}

std::size_t CommandStack::RedoCount() const
{
    return m_redo.size();
}

std::size_t CommandStack::MemoryBytes() const
{
    return m_memoryBytes;
}
} // namespace Editor::Editing

#endif // _EDITOR
