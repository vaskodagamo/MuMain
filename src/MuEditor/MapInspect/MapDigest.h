#pragma once

#ifdef _EDITOR

#include "MapObjectRecord.h"
#include "TerrainView.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// Fingerprints of the parts of a map that are saved to separate files, so the editor
// can tell which of them hold edits that were not saved (map-info's "unsaved").
namespace Editor::MapInspect
{
// The files a map is saved to.
enum class SaveUnit : std::uint8_t
{
    Texture,   // EncTerrain{N}.map: both tile layers and layer 2's opacity
    Height,    // TerrainHeight.OZB
    Attribute, // EncTerrain{N}.att
    Light,     // TerrainLight.OZJ
    Objects,   // EncTerrain{N}.obj
};

constexpr std::size_t SAVE_UNIT_COUNT = 5;

// texture, height, attribute, light, objects.
std::string_view SaveUnitName(SaveUnit unit);

using MapDigests = std::array<std::uint64_t, SAVE_UNIT_COUNT>;

// A 64-bit FNV-1a hash of `bytes`, continuing from `seed`.
std::uint64_t DigestBytes(const void* bytes, std::size_t size, std::uint64_t seed);
std::uint64_t DigestBytes(const void* bytes, std::size_t size);

// The same for the terrain layers each unit saves (the attribute without the bit the
// game sets for characters at run time) and for the objects, whose digest does not
// depend on the order they are listed in.
MapDigests DigestMap(const TerrainView& terrain, const std::vector<MapObjectRecord>& objects);
std::uint64_t DigestObjects(const std::vector<MapObjectRecord>& objects);
} // namespace Editor::MapInspect

#endif // _EDITOR
