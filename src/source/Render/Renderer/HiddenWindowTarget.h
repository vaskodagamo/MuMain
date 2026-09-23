#pragma once

// Frames for a window nobody can see, in builds with the control socket.
//
// A scripted client (the control socket) keeps driving the game while its window is
// minimized, hidden or fully covered by other windows. Waiting for the window's next
// swapchain image then can stall the whole main loop (the compositor stops handing
// images out on some platforms), and a skipped frame leaves a screenshot with no
// pixels. While the socket serves, such frames are drawn into an offscreen colour
// target of the window's size instead: the loop keeps its pace, and captures read the
// world as usual. Those frames are paced as a window would pace them (SubmitPaced):
// without a swapchain to wait for, the loop would run flat out and the GPU driver would
// pile up buffers for frames it has not drawn yet (tens of GB within seconds on macOS).
// A player build compiles none of this.
#if MU_ENABLE_CONTROL_SOCKET

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>

namespace Render::HiddenWindow
{
// Turned on by the control socket while it serves; off by default.
void SetEnabled(bool enabled);

// True when this frame should not wait for the window: drawing while hidden is on
// and the window is minimized, hidden or fully covered.
bool ShouldBypassWindow(SDL_Window* window);

// The offscreen target in the window's swapchain format, `width` x `height` (created
// or resized as needed); nullptr when it cannot be made.
SDL_GPUTexture* Target(SDL_GPUDevice* device, SDL_Window* window, Uint32 width, Uint32 height);

// Frees the target (renderer shutdown).
void Release(SDL_GPUDevice* device);

// Submits the commands of a frame drawn into the target, at the pace a visible window's
// swapchain and vsync would set: at most two frames queued on the GPU (the oldest is
// waited for), and at most one frame per 1/60 s (the rest of the interval is slept).
void SubmitPaced(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commands);

// Waits for the offscreen frames still on the GPU and lets their fences go: the window
// draws again, or the renderer shuts down.
void FinishFrames(SDL_GPUDevice* device);
} // namespace Render::HiddenWindow

#endif // MU_ENABLE_CONTROL_SOCKET
