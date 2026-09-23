#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <vector>

// The items selected in the Item Editor's Browse tab (item types), for actions on
// several items at once ("Generate concepts (N)..."). The primary item is the one
// the details panel shows; it stays shown when the selection is cleared. A plain
// click selects one item, Cmd-click (Ctrl-click off the Mac) adds or removes one,
// Shift-click selects the range between the last clicked item (the anchor) and
// the clicked one in the order the list shows. No ImGui, no engine state.
namespace Editor::Editing
{
struct ClickModifiers
{
    bool toggle = false; // Cmd (Mac) / Ctrl
    bool range = false;  // Shift
};

class ItemSelection
{
public:
    static constexpr int NONE = -1;

    // The item the details panel shows, NONE before the first click.
    int Primary() const { return m_primary; }
    // Selected items in the order they were added.
    const std::vector<int>& Types() const { return m_types; }
    std::size_t Count() const { return m_types.size(); }
    bool Contains(int type) const;

    // A click on `type`; `shown` is the list's order (for a Shift range).
    void Click(int type, ClickModifiers modifiers, const std::vector<int>& shown);
    // A click on the checkbox of `type`: adds or removes it, like Cmd-click.
    void Toggle(int type);
    // Only `type`, which becomes the primary and the anchor (e.g. picked in the Stats table).
    void SelectOnly(int type);
    // Every shown item; the primary is kept when it is shown.
    void SelectAll(const std::vector<int>& shown);
    // Nothing selected; the primary stays shown in the details panel.
    void Clear();

private:
    void Add(int type);
    void Remove(int type);
    void SelectRange(int from, int to, const std::vector<int>& shown);

    std::vector<int> m_types;
    int m_primary = NONE;
    int m_anchor = NONE;
};
} // namespace Editor::Editing

#endif // _EDITOR
