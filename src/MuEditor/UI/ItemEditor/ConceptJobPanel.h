#pragma once

#ifdef _EDITOR

// The window that follows the background concepts job (ConceptJob.h): per item
// waiting / generating / retry in N s / done / failed with the tool's message, the
// progress, Cancel (requests already sent finish), Resume after a cancel or
// failures, and at the end the actual cost next to the estimate. Shown while a job
// runs or until its result is dismissed, whether or not the dialog that started it
// is still open.
namespace Editor::ItemEditor::ConceptJobPanel
{
// Draws the window; call once per frame after the Item Editor window.
void Render();
} // namespace Editor::ItemEditor::ConceptJobPanel

#endif // _EDITOR
