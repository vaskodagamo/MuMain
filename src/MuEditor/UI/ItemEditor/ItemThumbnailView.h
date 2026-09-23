#pragma once

#ifdef _EDITOR

#include "Assets/ItemBrowse.h"

// The item model thumbnails of the Browse tab (rendered by CObjectThumbnail with
// item framing, cached per model).
namespace Editor::ItemEditor
{
// Draws the item's thumbnail as a size x size image. Asks for it on first use
// (it appears a frame later) and draws an empty frame until then, or when the
// item has no model loaded.
void DrawItemThumbnail(const Items::BrowseRow& row, float size);
} // namespace Editor::ItemEditor

#endif // _EDITOR
