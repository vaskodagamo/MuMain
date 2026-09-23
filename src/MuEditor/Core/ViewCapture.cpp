#include "stdafx.h"

#ifdef _EDITOR

#include "ViewCapture.h"

#include "Render/Renderer/MuRenderer.h"

#include "imgui.h"

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
};

// The renderer holds one read-back at a time (a PrintScreen screenshot or a
// diagnostic capture may be using it); give up after this many busy frames.
constexpr int MAX_BUSY_FRAMES = 30;
// A frame the GPU had no swapchain image for is skipped and read back as
// nothing (common with a fast frame rate); the next frame is taken instead.
constexpr int MAX_ATTEMPTS = 5;

State s_state = State::Idle;
int s_captureFrame = -1;
int s_busyFrames = 0;
int s_attempts = 0;
} // namespace

bool Request()
{
    if (s_state == State::Requested || s_state == State::Reading)
        return false;
    s_state = State::Requested;
    s_busyFrames = 0;
    s_attempts = 0;
    return true;
}

void BeginFrame()
{
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
    return s_state == State::Reading && ImGui::GetFrameCount() == s_captureFrame;
}

Result Collect(mu::FramePixels& pixels)
{
    if (s_state == State::Requested)
        return Result::Waiting;
    if (s_state == State::Reading && ImGui::GetFrameCount() <= s_captureFrame)
        return Result::Waiting;

    const bool read = s_state == State::Reading && mu::GetRenderer().ConsumeFramePixels(pixels);
    if (!read && s_state == State::Reading && ++s_attempts < MAX_ATTEMPTS)
    {
        s_state = State::Requested; // the frame was skipped: take the next one
        return Result::Waiting;
    }
    s_state = State::Idle;
    return read ? Result::Ready : Result::Failed;
}
} // namespace Editor::ViewCapture

#endif // _EDITOR
