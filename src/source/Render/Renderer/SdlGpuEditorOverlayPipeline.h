#pragma once

#include <SDL3/SDL_gpu.h>

namespace Render::EditorOverlay
{
// Formats of the render pass the editor overlay (Dear ImGui) is drawn into.
struct PassTargetFormats
{
    SDL_GPUTextureFormat color;
    SDL_GPUTextureFormat depthStencil;
};

// Creates the pipeline the editor passes to ImGui_ImplSDLGPU3_RenderDrawData.
// It draws like the pipeline imgui_impl_sdlgpu3 builds for itself (same
// shaders, vertex layout, blend and raster state), but it declares the pass's
// depth-stencil attachment, which ImGui's own pipeline leaves out. Depth test
// and depth write are off, so the overlay neither reads nor changes the
// world's depth. Editor builds only (the definition is behind _EDITOR).
// Returns nullptr on failure, with the reason in SDL_GetError(). The caller
// releases the pipeline with SDL_ReleaseGPUGraphicsPipeline.
[[nodiscard]] SDL_GPUGraphicsPipeline* CreatePipeline(SDL_GPUDevice* device, const PassTargetFormats& formats);
} // namespace Render::EditorOverlay
