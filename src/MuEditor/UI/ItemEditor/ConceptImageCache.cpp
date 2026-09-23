#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptImageCache.h"

#include "Assets/CaptureImage.h"
#include "Assets/EditorText.h"
#include "Core/EditorFiles.h"
#include "Render/Renderer/MuRenderer.h"

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>

#include <algorithm>
#include <chrono>
#include <vector>

namespace
{
// Frames an image may go undrawn before its texture is freed (about two seconds).
constexpr int FRAMES_KEPT = 120;
constexpr int MAX_UPLOADS_PER_FRAME = 2;
constexpr int MAX_DECODES_RUNNING = 3;
constexpr std::uint8_t BACKDROP[] = {36, 36, 40}; // transparent pixels, like the panel behind
constexpr std::size_t RGB_BYTES = 3;
constexpr std::size_t RGBA_BYTES = 4;
constexpr unsigned OPAQUE = 255;
constexpr std::uint8_t JPEG_MAGIC[] = {0xFF, 0xD8};

std::string EntryKey(const std::string& path, int maxSide)
{
    return path + "#" + std::to_string(maxSide);
}

std::uint8_t Blend(std::uint8_t color, std::uint8_t backdrop, unsigned alpha)
{
    return static_cast<std::uint8_t>((color * alpha + backdrop * (OPAQUE - alpha) + OPAQUE / 2) / OPAQUE);
}

// A PNG with its transparency laid on the backdrop; empty when SDL cannot read it.
mu::FramePixels LoadPng(const std::string& path)
{
    mu::FramePixels frame;
    SDL_Surface* loaded = SDL_LoadPNG(path.c_str());
    if (loaded == nullptr)
        return frame;
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr)
        return frame;
    frame.width = static_cast<std::uint32_t>(rgba->w);
    frame.height = static_cast<std::uint32_t>(rgba->h);
    frame.rgb.resize(static_cast<std::size_t>(frame.width) * frame.height * RGB_BYTES);
    for (std::uint32_t y = 0; y < frame.height; ++y)
    {
        const auto* row = static_cast<const std::uint8_t*>(rgba->pixels) + static_cast<std::size_t>(y) * rgba->pitch;
        std::uint8_t* out = frame.rgb.data() + static_cast<std::size_t>(y) * frame.width * RGB_BYTES;
        for (std::uint32_t x = 0; x < frame.width; ++x, row += RGBA_BYTES, out += RGB_BYTES)
        {
            for (std::size_t c = 0; c < RGB_BYTES; ++c)
                out[c] = Blend(row[c], BACKDROP[c], row[RGB_BYTES]);
        }
    }
    SDL_DestroySurface(rgba);
    return frame;
}

bool IsJpeg(const std::vector<unsigned char>& bytes)
{
    return bytes.size() > sizeof(JPEG_MAGIC) && std::equal(std::begin(JPEG_MAGIC), std::end(JPEG_MAGIC), bytes.begin());
}
} // namespace

CConceptImageCache& CConceptImageCache::GetInstance()
{
    static CConceptImageCache instance;
    return instance;
}

CConceptImageCache::Decoded CConceptImageCache::Decode(const std::string& path, int maxSide)
{
    mu::FramePixels frame;
    const std::vector<unsigned char> bytes = Editor::Files::ReadWholeFile(Editor::Text::Utf8Path(path));
    if (IsJpeg(bytes))
        Editor::Capture::DecodeJpeg(std::vector<std::uint8_t>(bytes.begin(), bytes.end()), frame);
    else if (!bytes.empty())
        frame = LoadPng(path);
    Decoded decoded;
    if (frame.width == 0 || frame.height == 0)
        return decoded;
    // Scale so the longer side fits (DownscaleToWidth limits the width only).
    const std::uint32_t longest = std::max(frame.width, frame.height);
    const auto side = static_cast<std::uint32_t>(std::max(1, maxSide));
    const std::uint32_t width = longest > side ? std::max<std::uint32_t>(1, frame.width * side / longest) : frame.width;
    const mu::FramePixels scaled = Editor::Capture::DownscaleToWidth(frame, width);
    decoded.width = scaled.width;
    decoded.height = scaled.height;
    decoded.rgba = Editor::Capture::ToRgba(scaled);
    return decoded;
}

void CConceptImageCache::BeginFrame()
{
    ++m_frame;
    m_uploadsThisFrame = 0;
    for (auto it = m_entries.begin(); it != m_entries.end();)
    {
        Entry& entry = it->second;
        const bool stale = m_frame - entry.lastUsedFrame > FRAMES_KEPT && !entry.pending.valid();
        if (!stale)
        {
            ++it;
            continue;
        }
        if (entry.image.texture != 0)
            mu::GetRenderer().ReleaseTexture(entry.image.texture);
        it = m_entries.erase(it);
    }
}

const CConceptImageCache::Image& CConceptImageCache::Get(const std::string& path, int maxSide)
{
    Entry& entry = m_entries[EntryKey(path, maxSide)];
    entry.lastUsedFrame = m_frame;
    if (entry.path.empty())
        entry.path = path;
    const bool idle = entry.image.texture == 0 && !entry.image.failed && !entry.pending.valid();
    if (idle && m_decodesRunning < MAX_DECODES_RUNNING)
    {
        ++m_decodesRunning;
        entry.pending = std::async(std::launch::async, [path, maxSide] { return Decode(path, maxSide); });
    }
    Upload(entry);
    return entry.image;
}

void CConceptImageCache::Upload(Entry& entry)
{
    using namespace std::chrono_literals;
    if (!entry.pending.valid() || m_uploadsThisFrame >= MAX_UPLOADS_PER_FRAME ||
        entry.pending.wait_for(0s) != std::future_status::ready)
        return;
    --m_decodesRunning;
    const Decoded decoded = entry.pending.get();
    if (decoded.rgba.empty())
    {
        entry.image.failed = true;
        return;
    }
    ++m_uploadsThisFrame;
    entry.image.texture = mu::GetRenderer().CreateTexture(decoded.width, decoded.height, decoded.rgba.data());
    entry.image.width = static_cast<int>(decoded.width);
    entry.image.height = static_cast<int>(decoded.height);
    entry.image.failed = entry.image.texture == 0;
}

void CConceptImageCache::Forget(const std::string& path)
{
    for (auto& [key, entry] : m_entries)
    {
        if (entry.path == path)
            entry.lastUsedFrame = -FRAMES_KEPT - 1; // freed at the next BeginFrame
    }
}

void CConceptImageCache::ReleaseAll()
{
    for (auto& [key, entry] : m_entries)
    {
        if (entry.pending.valid())
            entry.pending.wait();
        if (entry.image.texture != 0)
            mu::GetRenderer().ReleaseTexture(entry.image.texture);
    }
    m_entries.clear();
    m_decodesRunning = 0;
}

#endif // _EDITOR
