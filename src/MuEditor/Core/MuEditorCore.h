#pragma once

#ifdef _EDITOR

#include "stdafx.h"

#include "Editing/PopupMouseGuard.h"

struct SDL_Window;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;

class CMuEditorCore
{
public:
    static CMuEditorCore& GetInstance();

    // window is the SDL window used by the ImGui SDL3 backend (issue #442).
    void Initialize(SDL_Window* window);
    void Shutdown();
    void Update();
    void Render();
    void PrepareDrawData(SDL_GPUCommandBuffer* commandBuffer);
    void RenderDrawData(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* renderPass);

    bool IsEnabled() const { return m_bEditorMode; }
    void SetEnabled(bool enabled) { m_bEditorMode = enabled; }
    void ToggleEditor() { m_bEditorMode = !m_bEditorMode; }

    bool IsShowingItemEditor() const { return m_bShowItemEditor; }
    bool IsShowingSkillEditor() const { return m_bShowSkillEditor; }
    bool IsShowingDevEditor() const { return m_bShowDevEditor; }
    bool IsShowingMapEditor() const { return m_bShowMapEditor; }
    bool IsShowingConsole() const { return m_bShowConsole; }
    void ShowMapEditor()
    {
        m_bShowMapEditor = true;
    }
    void ShowItemEditor()
    {
        m_bShowItemEditor = true;
    }
    bool IsHoveringUI() const { return m_bHoveringUI; }
    void SetHoveringUI(bool hovering) { m_bHoveringUI = hovering; }
    // This frame shows the OS pointer instead of the game's cursor sprite: the mouse is
    // over an editor window, editor UI owns it, or the item studio is up (its preview and
    // gaps count as UI). Over the 3D world the game draws its own.
    bool WantsOsCursor() const { return m_bWantsOsCursor; }

    // Global scale for ALL editor UI (fonts + widget/padding sizes). Driven by the
    // -/+ buttons in the toolbar. Applied between frames, not mid-frame.
    void  SetUIScale(float scale);
    float GetUIScale() const { return m_UIScale; }
    // The owner's choice on the toolbar: applied and kept for the next start (MuEditor.ini).
    void ChooseUIScale(float scale);

    // The item studio's full screen (toolbar button and shortcut; remembered).
    bool IsFullscreen() const;
    void ToggleFullscreen();

    // The game window the editor draws into; OS dialogs (file pickers) attach to it.
    // nullptr until Initialize() succeeded.
    SDL_Window* GetWindow() const
    {
        return m_pWindow;
    }

private:
    CMuEditorCore();
    ~CMuEditorCore();

    void ApplyUIScale();
    void RememberConsoleChoice();
    // The studio's remembered window state and first UI scale (once the offline studio is up).
    void UpdateStudioPreferences();
    // Editor UI has the mouse this frame (see WantsOsCursor()); inside the ImGui frame.
    bool IsMouseOverEditorUI() const;
    // The game's cursor sprite and the OS pointer for this frame.
    void UpdateCursors(bool captureFrame);
    // The editor windows of this frame; with the editor closed only the Map Editor's
    // hand-back of the game's edit mode.
    void RenderEditorWindows();
    // Esc closes the open menus, combo lists and pickers (not dialogs), as ImGui's keyboard
    // navigation would; it is off because the arrow keys fly the camera.
    void CloseMenusOnEscape();

    bool m_bEditorMode;
    bool m_bInitialized;
    bool m_bFrameStarted;
    bool m_bDrawDataReady;
    bool m_bShowItemEditor;
    bool m_bShowSkillEditor;
    bool m_bShowDevEditor;
    bool m_bShowMapEditor;
    bool m_bShowConsole;
    bool m_bHoveringUI;
    bool m_bPreviousFrameHoveringUI;  // Store previous frame's hover state for input blocking
    bool m_bWantsOsCursor = false;    // this frame's choice, see WantsOsCursor()
    bool m_bOsCursorShown = false;    // what ShowCursor() was last forced to
    bool m_bOsCursorApplied = false;  // forced at least once (the game's start state is not known)
    Editor::Editing::PopupMouseGuard m_popupMouseGuard; // an open popup and the click that closes it keep the mouse

    float m_UIScale;        // 1.0 = default ImGui size
    bool  m_bScaleDirty;    // apply the new scale at the start of the next frame
    bool  m_bStudioScaleChecked = false; // the studio's first-start UI scale was considered

    SDL_Window* m_pWindow; // game window passed to Initialize()
};

// Global accessor
#define g_MuEditorCore CMuEditorCore::GetInstance()

#endif // _EDITOR
