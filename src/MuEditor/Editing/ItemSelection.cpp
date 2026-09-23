#include "ItemSelection.h"

#ifdef _EDITOR

#include <algorithm>

namespace Editor::Editing
{
bool ItemSelection::Contains(int type) const
{
    return std::find(m_types.begin(), m_types.end(), type) != m_types.end();
}

void ItemSelection::Click(int type, ClickModifiers modifiers, const std::vector<int>& shown)
{
    const bool anchorShown = std::find(shown.begin(), shown.end(), m_anchor) != shown.end();
    if (modifiers.range && anchorShown)
    {
        if (!modifiers.toggle)
            m_types.clear();
        SelectRange(m_anchor, type, shown);
        m_primary = type;
        return;
    }
    if (modifiers.toggle)
    {
        Toggle(type);
        return;
    }
    SelectOnly(type);
}

void ItemSelection::Toggle(int type)
{
    m_anchor = type;
    if (!Contains(type))
    {
        Add(type);
        m_primary = type;
        return;
    }
    Remove(type);
    if (m_primary == type && !m_types.empty())
        m_primary = m_types.back();
}

void ItemSelection::SelectOnly(int type)
{
    m_types.assign(1, type);
    m_primary = type;
    m_anchor = type;
}

void ItemSelection::SelectAll(const std::vector<int>& shown)
{
    m_types = shown;
    if (!shown.empty() && !Contains(m_primary))
        m_primary = shown.front();
}

void ItemSelection::Clear()
{
    m_types.clear();
}

void ItemSelection::Add(int type)
{
    if (!Contains(type))
        m_types.push_back(type);
}

void ItemSelection::Remove(int type)
{
    m_types.erase(std::remove(m_types.begin(), m_types.end(), type), m_types.end());
}

void ItemSelection::SelectRange(int from, int to, const std::vector<int>& shown)
{
    const auto first = std::find(shown.begin(), shown.end(), from);
    const auto last = std::find(shown.begin(), shown.end(), to);
    if (first == shown.end() || last == shown.end())
    {
        Add(to);
        return;
    }
    const auto begin = std::min(first, last);
    const auto end = std::max(first, last) + 1;
    for (auto it = begin; it != end; ++it)
        Add(*it);
}
} // namespace Editor::Editing

#endif // _EDITOR
