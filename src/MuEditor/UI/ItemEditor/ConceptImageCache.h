#pragma once

#ifdef _EDITOR

#include <cstdint>
#include <future>
#include <map>
#include <string>
#include <vector>

// The concept images the Item Editor shows (PNG variants from out/item-concepts,
// the picked concept.jpg, reference renders) as GPU textures: decoded and scaled
// on a worker thread, uploaded on the main thread a few per frame, and freed once
// they have not been drawn for a while (scrolled away, another item selected).
// Transparent pixels are laid on the panel's dark grey.
class CConceptImageCache
{
public:
    struct Image
    {
        std::uint32_t texture = 0; // renderer texture id; 0 while loading or failed
        int width = 0;
        int height = 0;
        bool failed = false;
    };

    static CConceptImageCache& GetInstance();

    // Call once per frame before any image of the frame is drawn: frees the
    // textures that were not used lately.
    void BeginFrame();
    // The image at `path` no larger than `maxSide` pixels; starts loading it the
    // first time. The texture is valid for this frame.
    const Image& Get(const std::string& path, int maxSide);
    // Loads `path` again the next time it is asked for (the file changed).
    void Forget(const std::string& path);
    // Frees every texture (the editor closes).
    void ReleaseAll();

private:
    struct Decoded
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> rgba;
    };
    struct Entry
    {
        std::string path;
        Image image;
        std::future<Decoded> pending;
        int lastUsedFrame = 0;
    };

    CConceptImageCache() = default;
    void Upload(Entry& entry);
    static Decoded Decode(const std::string& path, int maxSide);

    std::map<std::string, Entry> m_entries; // by path and size
    int m_frame = 0;
    int m_uploadsThisFrame = 0;
    int m_decodesRunning = 0;
};

#define g_ConceptImages CConceptImageCache::GetInstance()

#endif // _EDITOR
