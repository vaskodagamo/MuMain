#include "PopupMouseGuard.h"

#ifdef _EDITOR

namespace Editor::Editing
{
bool PopupMouseGuard::Update(bool popupOpen, bool buttonDown)
{
    if (popupOpen)
    {
        m_buttonFromPopup = buttonDown;
        return true;
    }
    m_buttonFromPopup = m_buttonFromPopup && buttonDown;
    return m_buttonFromPopup;
}
} // namespace Editor::Editing

#endif // _EDITOR
