#include "stdafx.h"

#ifdef _EDITOR

#include "MapEditorStatusLine.h"

#include "imgui.h"

namespace Editor::StatusLine
{
namespace
{
const ImVec4 STATUS_COLOR(0.6f, 1.0f, 0.6f, 1.0f);
const ImVec4 WARNING_COLOR(1.0f, 0.8f, 0.3f, 1.0f);
} // namespace

void Render(const std::string& text)
{
    if (text.empty())
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, STATUS_COLOR);
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
}

const ImVec4& WarningColor()
{
    return WARNING_COLOR;
}
} // namespace Editor::StatusLine

#endif // _EDITOR
