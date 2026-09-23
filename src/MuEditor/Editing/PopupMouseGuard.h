#pragma once

#ifdef _EDITOR

namespace Editor::Editing
{
// Decides once per frame whether the mouse belongs to an open ImGui popup (a menu, a
// combo's list, the colour picker, a dialog) instead of the world behind the editor's
// windows. While a popup is open ImGui reports every other window as not hovered, and it
// closes the popup on the click that lands outside it; without the guard that click
// would also select, place or paint whatever lies under the window it landed on. The
// mouse stays with the popup until every button held while it was open is released, so
// the click that closes it never reaches the world, also a press held a little longer.
class PopupMouseGuard
{
public:
    // `popupOpen`: a popup is open this frame; `buttonDown`: a mouse button is held.
    // Returns true while the world must not get the mouse.
    bool Update(bool popupOpen, bool buttonDown);

private:
    bool m_buttonFromPopup = false; // a button held while a popup was open is still down
};
} // namespace Editor::Editing

#endif // _EDITOR
