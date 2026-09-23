#include "EditCommandGroup.h"

#ifdef _EDITOR

#include <algorithm>
#include <utility>

namespace Editor::Editing
{
EditCommandGroup::EditCommandGroup(std::string label, std::vector<std::unique_ptr<EditCommand>> parts)
    : EditCommand(std::move(label))
{
    for (std::unique_ptr<EditCommand>& part : parts)
    {
        if (part != nullptr)
            m_parts.push_back(std::move(part));
    }
}

bool EditCommandGroup::Undo()
{
    bool ok = true;
    for (auto part = m_parts.rbegin(); part != m_parts.rend(); ++part)
        ok = (*part)->Undo() && ok;
    return ok;
}

bool EditCommandGroup::Redo()
{
    bool ok = true;
    for (const std::unique_ptr<EditCommand>& part : m_parts)
        ok = part->Redo() && ok;
    return ok;
}

std::size_t EditCommandGroup::MemoryBytes() const
{
    std::size_t bytes = sizeof(*this) + Label().capacity() + m_parts.capacity() * sizeof(m_parts[0]);
    for (const std::unique_ptr<EditCommand>& part : m_parts)
        bytes += part->MemoryBytes();
    return bytes;
}

std::unique_ptr<EditCommand> GroupEdits(std::string label, std::vector<std::unique_ptr<EditCommand>> parts)
{
    parts.erase(std::remove(parts.begin(), parts.end(), nullptr), parts.end());
    if (parts.empty())
        return nullptr;
    if (parts.size() == 1)
        return std::move(parts.front());
    return std::make_unique<EditCommandGroup>(std::move(label), std::move(parts));
}
} // namespace Editor::Editing

#endif // _EDITOR
