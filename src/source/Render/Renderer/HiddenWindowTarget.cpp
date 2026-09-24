#include "HiddenWindowTarget.h"

#if MU_ENABLE_CONTROL_SOCKET

#include "Core/Utilities/Log/MuLogger.h"

#include <chrono>
#include <deque>
#include <thread>

namespace Render::HiddenWindow
{
namespace
{
constexpr const char* LOG_CHANNEL = "render";
constexpr SDL_WindowFlags NOT_VISIBLE_FLAGS = SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED;
// A window's swapchain lets this many frames wait on the GPU before the next one blocks.
constexpr std::size_t MAX_FRAMES_IN_FLIGHT = 2;
// Vsync's pace on a 60 Hz display, the slowest a visible window runs.
constexpr std::chrono::microseconds FRAME_INTERVAL{16667};
constexpr bool WAIT_FOR_ALL = true;

struct State
{
    bool enabled = false;
    bool bypassing = false; // the last frame went to the offscreen target
    SDL_GPUTexture* target = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    std::deque<SDL_GPUFence*> framesInFlight; // oldest first
    std::chrono::steady_clock::time_point lastFrame{};
    bool fenceFailureLogged = false;
};

State& Current()
{
    static State state;
    return state;
}

// One line each way, so the log says when the socket kept a hidden window drawing.
void NoteBypass(bool bypassing)
{
    State& state = Current();
    if (state.bypassing == bypassing)
        return;
    state.bypassing = bypassing;
    if (bypassing)
        mu::log::Get(LOG_CHANNEL)->info("SDL_gpu -- window not visible: drawing offscreen for the control socket");
    else
        mu::log::Get(LOG_CHANNEL)->info("SDL_gpu -- window visible again: drawing to the window");
}

void WaitForOldestFrame(SDL_GPUDevice* device)
{
    State& state = Current();
    SDL_GPUFence* oldest = state.framesInFlight.front();
    state.framesInFlight.pop_front();
    SDL_WaitForGPUFences(device, WAIT_FOR_ALL, &oldest, 1);
    SDL_ReleaseGPUFence(device, oldest);
}

// Once: a frame without a fence is still paced by time, not by the GPU.
void NoteFenceFailure()
{
    State& state = Current();
    if (state.fenceFailureLogged)
        return;
    state.fenceFailureLogged = true;
    mu::log::Get(LOG_CHANNEL)->warn("SDL_gpu -- no fence for an offscreen frame: {}", SDL_GetError());
}

// Sleeps out the rest of the frame interval since the last offscreen frame.
void KeepFramePace()
{
    State& state = Current();
    const auto due = state.lastFrame + FRAME_INTERVAL;
    if (std::chrono::steady_clock::now() < due)
        std::this_thread::sleep_until(due);
    state.lastFrame = std::chrono::steady_clock::now();
}
} // namespace

void SetEnabled(bool enabled)
{
    Current().enabled = enabled;
}

bool ShouldBypassWindow(SDL_Window* window)
{
    const bool bypass = Current().enabled && window != nullptr && (SDL_GetWindowFlags(window) & NOT_VISIBLE_FLAGS) != 0;
    NoteBypass(bypass);
    return bypass;
}

SDL_GPUTexture* Target(SDL_GPUDevice* device, SDL_Window* window, Uint32 width, Uint32 height)
{
    State& state = Current();
    if (width == 0 || height == 0)
        return nullptr;
    if (state.target != nullptr && state.width == width && state.height == height)
        return state.target;

    Release(device);
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    state.target = SDL_CreateGPUTexture(device, &info);
    if (state.target == nullptr)
    {
        mu::log::Get(LOG_CHANNEL)->warn("SDL_gpu -- offscreen target for a hidden window failed: {}", SDL_GetError());
        return nullptr;
    }
    state.width = width;
    state.height = height;
    return state.target;
}

void Release(SDL_GPUDevice* device)
{
    State& state = Current();
    if (state.target != nullptr && device != nullptr)
        SDL_ReleaseGPUTexture(device, state.target);
    state.target = nullptr;
    state.width = 0;
    state.height = 0;
}

void SubmitPaced(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commands)
{
    State& state = Current();
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence == nullptr)
        NoteFenceFailure();
    else
        state.framesInFlight.push_back(fence);
    while (state.framesInFlight.size() > MAX_FRAMES_IN_FLIGHT)
        WaitForOldestFrame(device);
    KeepFramePace();
}

void FinishFrames(SDL_GPUDevice* device)
{
    while (!Current().framesInFlight.empty())
        WaitForOldestFrame(device);
}
} // namespace Render::HiddenWindow

#endif // MU_ENABLE_CONTROL_SOCKET
