#pragma once

#ifdef _EDITOR

#include <cstdint>
#include <functional>
#include <vector>

// The ciphers a map's EncTerrain files are stored with, handed in by the caller so the
// new-map units stay free of engine headers: MapFileEncrypt / MapFileDecrypt
// (Render/Terrain/ZzzLodTerrain.h) for .map, .att and .obj, and BuxConvert
// (Core/Globals/_crypt.h), which the .att also gets and which undoes itself.
namespace Editor::NewMap
{
using Bytes = std::vector<std::uint8_t>;

struct MapFileCodec
{
    std::function<Bytes(const Bytes&)> encrypt;
    std::function<Bytes(const Bytes&)> decrypt;
    std::function<void(Bytes&)> buxConvert;
};
} // namespace Editor::NewMap

#endif // _EDITOR
