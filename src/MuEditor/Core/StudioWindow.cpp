#include "stdafx.h"

#ifdef _EDITOR

#include "StudioWindow.h"

#include "Config/MuEditorConfig.h"

#include "imgui.h"

#include <SDL3/SDL_video.h>

namespace Editor::StudioWindow
{
namespace
{
constexpr float DEFAULT_SCALE = 1.0f;

bool s_applied = false;
bool s_lastKnown = false;

void Remember(bool fullscreen)
{
    s_lastKnown = fullscreen;
    if (g_MuEditorConfig.GetStudioFullscreen() == fullscreen)
        return;
    g_MuEditorConfig.SetStudioFullscreen(fullscreen);
    g_MuEditorConfig.Save();
}
} // namespace

bool IsFullscreen(SDL_Window* window)
{
    return window != nullptr && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void SetFullscreen(SDL_Window* window, bool fullscreen)
{
    if (window == nullptr)
        return;
    SDL_SetWindowFullscreenMode(window, nullptr); // the desktop's size, no display mode change
    SDL_SetWindowFullscreen(window, fullscreen);
    SDL_SyncWindow(window); // the change is asynchronous on macOS; the flags are read right after
    Remember(IsFullscreen(window));
}

bool FullscreenShortcutPressed()
{
#ifdef __APPLE__
    // ImGui reports Cmd as Ctrl and the Mac's Control key as Super.
    constexpr ImGuiKeyChord CHORD = ImGuiMod_Ctrl | ImGuiMod_Super | ImGuiKey_F;
#else
    constexpr ImGuiKeyChord CHORD = ImGuiKey_F11;
#endif
    return !ImGui::GetIO().WantTextInput && ImGui::IsKeyChordPressed(CHORD);
}

const char* FullscreenShortcutLabel()
{
#ifdef __APPLE__
    return "Cmd+Ctrl+F";
#else
    return "F11";
#endif
}

void Update(SDL_Window* window)
{
    if (window == nullptr)
        return;
    if (!s_applied)
    {
        s_applied = true;
        s_lastKnown = IsFullscreen(window);
        if (g_MuEditorConfig.GetStudioFullscreen() != s_lastKnown)
            SetFullscreen(window, g_MuEditorConfig.GetStudioFullscreen());
        return;
    }
    const bool now = IsFullscreen(window);
    if (now != s_lastKnown)
        Remember(now);
}

float DefaultUIScale(SDL_Window* window)
{
    SDL_Rect bounds{};
    const SDL_DisplayID display = window != nullptr ? SDL_GetDisplayForWindow(window) : 0;
    if (display == 0 || !SDL_GetDisplayBounds(display, &bounds))
        return DEFAULT_SCALE;
    return bounds.h >= LARGE_DISPLAY_HEIGHT ? LARGE_DISPLAY_SCALE : DEFAULT_SCALE;
}
} // namespace Editor::StudioWindow

#endif // _EDITOR
