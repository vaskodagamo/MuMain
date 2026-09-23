#pragma once

#ifdef _EDITOR

#include "Render/Renderer/FramePixelReadback.h" // mu::FramePixels

// A screenshot of the current view without the editor on top, for regeneration
// requests. The frame after the request is the capture frame: it is drawn without
// the ImGui overlay, the game cursor and the editor's on-screen camera text (the
// 3D view, including the selection outlines, stays as it is), and the renderer
// reads it back once the GPU has finished it. The editor panels are missing from
// that one frame only.
namespace Editor::ViewCapture
{
enum class Result
{
    Waiting, // the capture frame has not been read back yet
    Ready,   // pixels holds the frame
    Failed,  // no frame will come; start a new capture
};

// Asks for a capture of the next frame. False while one is running.
bool Request();

// Called by the editor core at the start of every frame, before the scene is
// drawn: makes this frame the capture frame when one was requested.
void BeginFrame();

// True while the capture frame is being built: leave editor-only overlays out.
bool IsCleanFrame();

// Collects the captured frame in a later frame.
Result Collect(mu::FramePixels& pixels);
} // namespace Editor::ViewCapture

#endif // _EDITOR
