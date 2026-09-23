#pragma once

#ifdef _EDITOR

class CMuEditorUI
{
public:
    static CMuEditorUI& GetInstance();

    void RenderToolbar(bool& editorEnabled, bool& showItemEditor, bool& showSkillEditor, bool& showDevEditor, bool& showMapEditor, bool& showConsole);
    void RenderCenterViewport();

    // The toolbar's height in pixels at the current editor UI scale.
    float ToolbarHeight() const;

private:
    CMuEditorUI() = default;
    ~CMuEditorUI() = default;

    void RenderToolbarOpen(bool& editorEnabled);
    void RenderToolbarFull(bool& editorEnabled, bool& showItemEditor, bool& showSkillEditor, bool& showDevEditor, bool& showMapEditor, bool& showConsole);
};

#define g_MuEditorUI CMuEditorUI::GetInstance()

#endif // _EDITOR
