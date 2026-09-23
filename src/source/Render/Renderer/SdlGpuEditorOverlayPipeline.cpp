// Editor-only. The shader choice and pipeline state below mirror
// ImGui_ImplSDLGPU3_CreateShaders and ImGui_ImplSDLGPU3_CreateGraphicsPipeline
// in ThirdParty/imgui/backends/imgui_impl_sdlgpu3.cpp; compare the two after
// an imgui submodule update.
#ifdef _EDITOR

#include "SdlGpuEditorOverlayPipeline.h"

#include "imgui.h"
#include "imgui_impl_sdlgpu3_shaders.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>

namespace
{
// imgui_impl_sdlgpu3's vertex shader reads one uniform buffer (scale and
// translation); its fragment shader samples one texture.
constexpr Uint32 kVertexUniformBufferCount = 1;
constexpr Uint32 kFragmentSamplerCount = 1;
constexpr Uint32 kVertexBufferSlot = 0;
constexpr std::size_t kVertexAttributeCount = 3;
constexpr const char* kDefaultEntryPoint = "main";
constexpr const char* kMetalEntryPoint = "main0";
constexpr const char* kVulkanDriver = "vulkan";
constexpr const char* kDirect3D12Driver = "direct3d12";

struct ShaderBlob
{
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const Uint8* code = nullptr;
    std::size_t size = 0;
    const char* entryPoint = kDefaultEntryPoint;
};

struct ShaderBlobPair
{
    ShaderBlob vertex;
    ShaderBlob fragment;
};

struct ShaderReleaser
{
    SDL_GPUDevice* device;

    void operator()(SDL_GPUShader* shader) const
    {
        SDL_ReleaseGPUShader(device, shader);
    }
};

using ShaderHandle = std::unique_ptr<SDL_GPUShader, ShaderReleaser>;
using VertexAttributes = std::array<SDL_GPUVertexAttribute, kVertexAttributeCount>;

// Picks the same precompiled blobs as ImGui_ImplSDLGPU3_CreateShaders.
ShaderBlobPair SelectShaderBlobs(SDL_GPUDevice* device)
{
    const char* driver = SDL_GetGPUDeviceDriver(device);
    if (driver == nullptr)
    {
        return {};
    }
    if (std::strcmp(driver, kVulkanDriver) == 0)
    {
        return {{SDL_GPU_SHADERFORMAT_SPIRV, spirv_vertex, sizeof(spirv_vertex)},
                {SDL_GPU_SHADERFORMAT_SPIRV, spirv_fragment, sizeof(spirv_fragment)}};
    }
    if (std::strcmp(driver, kDirect3D12Driver) == 0)
    {
        return {{SDL_GPU_SHADERFORMAT_DXBC, dxbc_vertex, sizeof(dxbc_vertex)},
                {SDL_GPU_SHADERFORMAT_DXBC, dxbc_fragment, sizeof(dxbc_fragment)}};
    }
#ifdef __APPLE__
    const SDL_GPUShaderFormat supportedFormats = SDL_GetGPUShaderFormats(device);
    if (supportedFormats & SDL_GPU_SHADERFORMAT_METALLIB)
    {
        return {{SDL_GPU_SHADERFORMAT_METALLIB, metallib_vertex, sizeof(metallib_vertex), kMetalEntryPoint},
                {SDL_GPU_SHADERFORMAT_METALLIB, metallib_fragment, sizeof(metallib_fragment), kMetalEntryPoint}};
    }
    if (supportedFormats & SDL_GPU_SHADERFORMAT_MSL)
    {
        return {{SDL_GPU_SHADERFORMAT_MSL, msl_vertex, sizeof(msl_vertex), kMetalEntryPoint},
                {SDL_GPU_SHADERFORMAT_MSL, msl_fragment, sizeof(msl_fragment), kMetalEntryPoint}};
    }
#endif
    return {};
}

ShaderHandle CreateShader(SDL_GPUDevice* device, const ShaderBlob& blob, SDL_GPUShaderStage stage)
{
    ShaderHandle shader(nullptr, ShaderReleaser{device});
    if (blob.code == nullptr)
    {
        SDL_SetError("Dear ImGui has no shader for GPU driver %s", SDL_GetGPUDeviceDriver(device));
        return shader;
    }

    const bool isVertexStage = stage == SDL_GPU_SHADERSTAGE_VERTEX;
    SDL_GPUShaderCreateInfo info{};
    info.code_size = blob.size;
    info.code = blob.code;
    info.entrypoint = blob.entryPoint;
    info.format = blob.format;
    info.stage = stage;
    info.num_samplers = isVertexStage ? 0 : kFragmentSamplerCount;
    info.num_uniform_buffers = isVertexStage ? kVertexUniformBufferCount : 0;
    shader.reset(SDL_CreateGPUShader(device, &info));
    return shader;
}

SDL_GPUVertexAttribute MakeVertexAttribute(Uint32 location, SDL_GPUVertexElementFormat format, std::size_t offset)
{
    SDL_GPUVertexAttribute attribute{};
    attribute.location = location;
    attribute.buffer_slot = kVertexBufferSlot;
    attribute.format = format;
    attribute.offset = static_cast<Uint32>(offset);
    return attribute;
}

// ImDrawVert: position and texture coordinate as float2, colour as 4 normalised bytes.
VertexAttributes MakeVertexAttributes()
{
    return {MakeVertexAttribute(0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(ImDrawVert, pos)),
            MakeVertexAttribute(1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(ImDrawVert, uv)),
            MakeVertexAttribute(2, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(ImDrawVert, col))};
}

SDL_GPUVertexBufferDescription MakeVertexBufferDescription()
{
    SDL_GPUVertexBufferDescription description{};
    description.slot = kVertexBufferSlot;
    description.pitch = sizeof(ImDrawVert);
    description.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    description.instance_step_rate = 0;
    return description;
}

// Straight alpha blending for colour; alpha accumulates towards opaque.
SDL_GPUColorTargetBlendState MakeBlendState()
{
    SDL_GPUColorTargetBlendState blend{};
    blend.enable_blend = true;
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.color_write_mask =
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    return blend;
}

SDL_GPURasterizerState MakeRasterizerState()
{
    SDL_GPURasterizerState rasterizer{};
    rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
    rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE;
    rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    rasterizer.enable_depth_bias = false;
    rasterizer.enable_depth_clip = false;
    return rasterizer;
}

// Because the pipeline carries its own depth-stencil state, SDL's Metal backend
// binds it with the pipeline, so the overlay never inherits the depth test or
// depth write of the last world draw.
SDL_GPUDepthStencilState MakeDepthStencilState()
{
    SDL_GPUDepthStencilState depthStencil{};
    depthStencil.enable_depth_test = false;
    depthStencil.enable_depth_write = false;
    depthStencil.enable_stencil_test = false;
    depthStencil.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
    return depthStencil;
}
} // namespace

namespace Render::EditorOverlay
{
SDL_GPUGraphicsPipeline* CreatePipeline(SDL_GPUDevice* device, const PassTargetFormats& formats)
{
    const ShaderBlobPair blobs = SelectShaderBlobs(device);
    const ShaderHandle vertexShader = CreateShader(device, blobs.vertex, SDL_GPU_SHADERSTAGE_VERTEX);
    const ShaderHandle fragmentShader = CreateShader(device, blobs.fragment, SDL_GPU_SHADERSTAGE_FRAGMENT);
    if (!vertexShader || !fragmentShader)
    {
        return nullptr;
    }

    const VertexAttributes vertexAttributes = MakeVertexAttributes();
    const SDL_GPUVertexBufferDescription vertexBuffer = MakeVertexBufferDescription();
    SDL_GPUColorTargetDescription colorTarget{};
    colorTarget.format = formats.color;
    colorTarget.blend_state = MakeBlendState();

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertexShader.get();
    info.fragment_shader = fragmentShader.get();
    info.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = vertexAttributes.data();
    info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(vertexAttributes.size());
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state = MakeRasterizerState();
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1; // the main pass is single-sampled
    info.depth_stencil_state = MakeDepthStencilState();
    info.target_info.color_target_descriptions = &colorTarget;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = formats.depthStencil;

    // The pipeline keeps what it needs; the shader handles release on return.
    return SDL_CreateGPUGraphicsPipeline(device, &info);
}
} // namespace Render::EditorOverlay

#endif // _EDITOR
