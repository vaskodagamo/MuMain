#include "ObjectSelection.h"

#ifdef _EDITOR

#include <algorithm>

namespace Editor::Editing
{
OBJECT* ObjectSelection::Primary() const
{
    return m_objects.empty() ? nullptr : m_objects.back();
}

bool ObjectSelection::IsEmpty() const
{
    return m_objects.empty();
}

std::size_t ObjectSelection::Count() const
{
    return m_objects.size();
}

bool ObjectSelection::Contains(const OBJECT* object) const
{
    return std::find(m_objects.begin(), m_objects.end(), object) != m_objects.end();
}

void ObjectSelection::Clear()
{
    m_objects.clear();
}

void ObjectSelection::SelectOnly(OBJECT* object)
{
    m_objects.clear();
    if (object != nullptr)
        m_objects.push_back(object);
}

void ObjectSelection::Toggle(OBJECT* object)
{
    if (Contains(object))
        Remove(object);
    else
        Add(object);
}

void ObjectSelection::Add(OBJECT* object)
{
    if (object == nullptr)
        return;
    Remove(object);
    m_objects.push_back(object);
}

void ObjectSelection::Remove(const OBJECT* object)
{
    m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), object), m_objects.end());
}

void ObjectSelection::Replace(OBJECT* from, OBJECT* to)
{
    if (from == to)
        return;
    if (to == nullptr)
    {
        Remove(from);
        return;
    }
    std::replace(m_objects.begin(), m_objects.end(), from, to);
}

void ObjectSelection::Assign(const std::vector<OBJECT*>& objects)
{
    m_objects.clear();
    for (OBJECT* object : objects)
    {
        if (object != nullptr && !Contains(object))
            m_objects.push_back(object);
    }
}
} // namespace Editor::Editing

#endif // _EDITOR
