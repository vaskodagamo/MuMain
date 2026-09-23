#pragma once

#ifdef _EDITOR

#include "Render/Renderer/MuRenderer.h"

// Ends the renderer's offscreen capture (BeginOffscreenCapture) when it goes out of
// scope, on every exit path. A capture left open makes BeginOffscreenCapture()
// refuse every later call, which would silently break every thumbnail and
// preview for the rest of the process.
class ScopedOffscreenCapture
{
public:
    ScopedOffscreenCapture() = default;
    ScopedOffscreenCapture(const ScopedOffscreenCapture&) = delete;
    ScopedOffscreenCapture& operator=(const ScopedOffscreenCapture&) = delete;
    ~ScopedOffscreenCapture() { mu::GetRenderer().EndOffscreenCapture(); }
};

#endif // _EDITOR
