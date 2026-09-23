#include "stdafx.h"

#ifdef _EDITOR

#include "ItemThumbnailView.h"

#include "ItemBrowseSource.h"
#include "UI/MapEditor/ObjectThumbnail.h"

#include "Render/Renderer/MuRenderer.h"

#include "imgui.h"

namespace Editor::ItemEditor
{
namespace
{
constexpr ImU32 EMPTY_FRAME_COLOR = IM_COL32(90, 90, 96, 160);

void DrawEmptyFrame(float size)
{
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));
    ImGui::GetWindowDrawList()->AddRect(corner, ImVec2(corner.x + size, corner.y + size), EMPTY_FRAME_COLOR);
}
} // namespace

void DrawItemThumbnail(const Items::BrowseRow& row, float size)
{
    if (!row.hasModel)
    {
        DrawEmptyFrame(size);
        return;
    }
    const unsigned int texture = g_ObjectThumbnail.Get(ModelTypeOf(row.type), Editor::Thumbnail::Framing::Item);
    void* const pointer = texture != 0 ? mu::GetRenderer().GetTexturePointer(texture) : nullptr;
    if (pointer == nullptr)
    {
        DrawEmptyFrame(size);
        return;
    }
    ImGui::Image((ImTextureID)(intptr_t)pointer, ImVec2(size, size));
}
} // namespace Editor::ItemEditor

#endif // _EDITOR
