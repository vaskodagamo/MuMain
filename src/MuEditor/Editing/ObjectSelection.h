#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <vector>

class OBJECT;

namespace Editor::Editing
{
// The Map Editor's selected world objects, in the order they were picked. The last
// one picked is the primary: the Objects tab shows its values. The selection only
// keeps pointers and never reads the objects, so it has no engine dependency; the
// caller keeps it valid (Replace when an object is re-created, Clear when the map's
// objects are freed).
class ObjectSelection
{
public:
    const std::vector<OBJECT*>& Objects() const
    {
        return m_objects;
    }
    OBJECT* Primary() const;
    bool IsEmpty() const;
    std::size_t Count() const;
    bool Contains(const OBJECT* object) const;

    void Clear();
    // A plain click: only `object` is selected.
    void SelectOnly(OBJECT* object);
    // A Shift/Cmd click: adds `object` as the primary, or takes it out when selected.
    void Toggle(OBJECT* object);
    // Adds `object` as the primary (or makes it the primary when already selected).
    void Add(OBJECT* object);
    void Remove(const OBJECT* object);
    // Keeps the selection when an edit re-created `from` as `to` (a null `to`
    // drops `from`).
    void Replace(OBJECT* from, OBJECT* to);
    // Selects exactly `objects`; the last one becomes the primary. Nulls and
    // repeats are left out.
    void Assign(const std::vector<OBJECT*>& objects);

private:
    std::vector<OBJECT*> m_objects;
};
} // namespace Editor::Editing

#endif // _EDITOR
