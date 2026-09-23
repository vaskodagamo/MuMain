#include "stdafx.h"

#ifdef _EDITOR

#include "ItemStudio.h"

#include "MuEditorCore.h"
#include "OfflineWorld.h"
#include "UI/MapEditor/ObjectThumbnail.h"

namespace Editor::ItemStudio
{
bool DocksItemEditor()
{
    return OfflineWorld::IsItemStudio() && g_MuEditorCore.IsEnabled() && g_MuEditorCore.IsShowingItemEditor() &&
           !g_MuEditorCore.IsShowingMapEditor();
}

bool ShowsBackdropOnly()
{
    return DocksItemEditor();
}

void RenderInsteadOfWorld()
{
    g_ObjectThumbnail.ProcessPendingRequests();
}
} // namespace Editor::ItemStudio

#endif // _EDITOR
