#pragma once

#ifdef _EDITOR

struct SDL_Window;

// The item studio's window preferences (--editor --items): full screen on and off
// (toolbar button, Cmd+Ctrl+F on a Mac, F11 elsewhere), and the editor UI scale it
// starts with on a large display. Both are kept in MuEditor/MuEditor.ini.
namespace Editor::StudioWindow
{
bool IsFullscreen(SDL_Window* window);
// Switches the window to full screen (the desktop's size) or back; remembered for the studio.
void SetFullscreen(SDL_Window* window, bool fullscreen);
// The full screen shortcut was pressed this frame.
bool FullscreenShortcutPressed();
// "Cmd+Ctrl+F" or "F11", for tooltips.
const char* FullscreenShortcutLabel();

// Once per frame in the studio: applies the remembered full screen state at start,
// and remembers a change made another way (the macOS window menu).
void Update(SDL_Window* window);

// The UI scale to start with when the owner never chose one: bigger on a display
// at least LARGE_DISPLAY_HEIGHT points high (a 1440p or 5K screen), else 1.
constexpr float LARGE_DISPLAY_SCALE = 1.25f;
constexpr int LARGE_DISPLAY_HEIGHT = 1440;
float DefaultUIScale(SDL_Window* window);
} // namespace Editor::StudioWindow

#endif // _EDITOR
