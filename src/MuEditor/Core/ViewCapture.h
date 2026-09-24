#pragma once

#ifdef _EDITOR

#include "Render/Renderer/FramePixelReadback.h" // mu::FramePixels

// A screenshot of the current view without the editor on top, for regeneration
// requests and the control socket's clean screenshots. The frame after the request is
// the capture frame: it is drawn without the ImGui overlay, the game cursor, the game
// HUD and the editor's on-screen camera text (the 3D view, including the selection
// outlines, stays as it is), and the renderer reads it back once the GPU has finished
// it. The editor panels are missing from that one frame only.
namespace Editor::ViewCapture
{
enum class Result
{
    Waiting, // the capture frame has not been read back yet
    Ready,   // pixels holds the frame
    Failed,  // no frame will come; start a new capture
};

// What the capture frame shows on top of the 3D view.
enum class Overlay
{
    Hidden, // nothing: no editor, cursor, HUD or camera text
    Shown,  // the frame as the window shows it
};

// Asks for a capture of the next frame. False while one is running.
bool Request(Overlay overlay = Overlay::Hidden);

// Called by the editor core at the start of every frame, before the scene is
// drawn: makes this frame the capture frame when one was requested.
void BeginFrame();

// True while the capture frame is being built without its overlay: leave editor-only
// overlays and the HUD out.
bool IsCleanFrame();

// Collects the captured frame in a later frame.
Result Collect(mu::FramePixels& pixels);

// The caller no longer wants the capture it requested (it gave up waiting). A frame
// already being read back is drained in a later frame, so the next capture gets its
// own frame.
void Cancel();
} // namespace Editor::ViewCapture

#endif // _EDITOR
