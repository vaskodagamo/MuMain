#include "stdafx.h"

#ifdef _EDITOR

#include "ViewCapture.h"

#include "Render/Renderer/MuRenderer.h"

#include "imgui.h"

#include <chrono>

namespace Editor::ViewCapture
{
namespace
{
enum class State
{
    Idle,
    Requested, // the next frame becomes the capture frame
    Reading,   // the capture frame is built and read back at its end
    Failed,
    Abandoned, // cancelled while its frame was read back: the pixels are drained
};

// The renderer holds one read-back at a time (a PrintScreen screenshot or a
// diagnostic capture may be using it); give up after this many busy frames.
constexpr int MAX_BUSY_FRAMES = 30;
// A frame the GPU had no swapchain image for is skipped and read back as
// nothing (common with a fast frame rate, and on macOS while a partly covered
// window gets no image before SDL reports it occluded); the next frame is taken
// instead, for this long after the request (the control socket's screenshot
// waits 5 s for its answer).
constexpr std::chrono::milliseconds SKIPPED_FRAME_PATIENCE{3000};
// A cancelled capture's frame is waited for this many frames before it is let go.
constexpr int MAX_DRAIN_FRAMES = 5;

State s_state = State::Idle;
Overlay s_overlay = Overlay::Hidden;
int s_captureFrame = -1;
int s_busyFrames = 0;
std::chrono::steady_clock::time_point s_requestedAt{};
int s_drainFrames = 0;

// Throws away the pixels of an abandoned capture once they arrive, so the renderer
// is free for the next read-back.
void DrainAbandoned()
{
    mu::FramePixels stale;
    const bool drained = ImGui::GetFrameCount() > s_captureFrame && mu::GetRenderer().ConsumeFramePixels(stale);
    if (drained || ++s_drainFrames >= MAX_DRAIN_FRAMES)
        s_state = State::Idle;
}
} // namespace

bool Request(Overlay overlay)
{
    if (s_state == State::Requested || s_state == State::Reading || s_state == State::Abandoned)
        return false;
    s_state = State::Requested;
    s_overlay = overlay;
    s_busyFrames = 0;
    s_requestedAt = std::chrono::steady_clock::now();
    return true;
}

void BeginFrame()
{
    if (s_state == State::Abandoned)
    {
        DrainAbandoned();
        return;
    }
    if (s_state != State::Requested)
        return;
    if (!mu::GetRenderer().RequestFramePixels())
    {
        if (++s_busyFrames >= MAX_BUSY_FRAMES)
            s_state = State::Failed;
        return;
    }
    s_state = State::Reading;
    s_captureFrame = ImGui::GetFrameCount();
}

bool IsCleanFrame()
{
    return s_state == State::Reading && s_overlay == Overlay::Hidden && ImGui::GetFrameCount() == s_captureFrame;
}

Result Collect(mu::FramePixels& pixels)
{
    if (s_state == State::Requested)
        return Result::Waiting;
    if (s_state == State::Reading && ImGui::GetFrameCount() <= s_captureFrame)
        return Result::Waiting;

    const bool read = s_state == State::Reading && mu::GetRenderer().ConsumeFramePixels(pixels);
    const bool patient = std::chrono::steady_clock::now() - s_requestedAt < SKIPPED_FRAME_PATIENCE;
    if (!read && s_state == State::Reading && patient)
    {
        s_state = State::Requested; // the frame was skipped: take the next one
        return Result::Waiting;
    }
    s_state = State::Idle;
    return read ? Result::Ready : Result::Failed;
}

void Cancel()
{
    if (s_state == State::Reading)
    {
        s_state = State::Abandoned;
        s_drainFrames = 0;
        return;
    }
    if (s_state == State::Requested || s_state == State::Failed)
        s_state = State::Idle;
}
} // namespace Editor::ViewCapture

#endif // _EDITOR
