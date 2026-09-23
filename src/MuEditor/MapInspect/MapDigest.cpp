#include "MapDigest.h"

#ifdef _EDITOR

#include "AttributePalette.h" // StoredAttribute

namespace Editor::MapInspect
{
namespace
{
constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
constexpr std::uint64_t FNV_PRIME = 1099511628211ull;
constexpr std::size_t LIGHT_CHANNELS = 3;

constexpr std::array<std::string_view, SAVE_UNIT_COUNT> UNIT_NAMES = {
    "texture", "height", "attribute", "light", "objects",
};

std::size_t UnitIndex(SaveUnit unit)
{
    return static_cast<std::size_t>(unit);
}

template <typename T> std::uint64_t DigestArray(const T* values, std::size_t count, std::uint64_t seed)
{
    return values == nullptr ? seed : DigestBytes(values, count * sizeof(T), seed);
}

std::uint64_t DigestTexture(const TerrainView& terrain)
{
    std::uint64_t digest = DigestArray(terrain.baseTiles, MAP_CELLS, FNV_OFFSET_BASIS);
    digest = DigestArray(terrain.overlayTiles, MAP_CELLS, digest);
    return DigestArray(terrain.overlayAlpha, MAP_CELLS, digest);
}

std::uint64_t DigestAttribute(const TerrainView& terrain)
{
    if (terrain.attribute == nullptr)
        return FNV_OFFSET_BASIS;
    std::uint64_t digest = FNV_OFFSET_BASIS;
    for (std::size_t cell = 0; cell < MAP_CELLS; ++cell)
    {
        const std::uint16_t stored = StoredAttribute(terrain.attribute[cell]);
        digest = DigestBytes(&stored, sizeof(stored), digest);
    }
    return digest;
}

std::uint64_t DigestObject(const MapObjectRecord& object)
{
    std::uint64_t digest = DigestBytes(&object.type, sizeof(object.type));
    digest = DigestBytes(object.position, sizeof(object.position), digest);
    digest = DigestBytes(object.angle, sizeof(object.angle), digest);
    return DigestBytes(&object.scale, sizeof(object.scale), digest);
}
} // namespace

std::string_view SaveUnitName(SaveUnit unit)
{
    return UNIT_NAMES[UnitIndex(unit)];
}

std::uint64_t DigestBytes(const void* bytes, std::size_t size, std::uint64_t seed)
{
    const auto* data = static_cast<const std::uint8_t*>(bytes);
    std::uint64_t digest = seed;
    for (std::size_t i = 0; i < size; ++i)
    {
        digest ^= data[i];
        digest *= FNV_PRIME;
    }
    return digest;
}

std::uint64_t DigestBytes(const void* bytes, std::size_t size)
{
    return DigestBytes(bytes, size, FNV_OFFSET_BASIS);
}

std::uint64_t DigestObjects(const std::vector<MapObjectRecord>& objects)
{
    // A sum of the objects' own digests: an undo that re-creates an object puts it
    // elsewhere in the object lists, which must not count as a change.
    std::uint64_t sum = 0;
    for (const MapObjectRecord& object : objects)
        sum += DigestObject(object);
    const std::uint64_t count = objects.size();
    return DigestBytes(&count, sizeof(count), sum);
}

MapDigests DigestMap(const TerrainView& terrain, const std::vector<MapObjectRecord>& objects)
{
    MapDigests digests{};
    digests[UnitIndex(SaveUnit::Texture)] = DigestTexture(terrain);
    digests[UnitIndex(SaveUnit::Height)] = DigestArray(terrain.height, MAP_CELLS, FNV_OFFSET_BASIS);
    digests[UnitIndex(SaveUnit::Attribute)] = DigestAttribute(terrain);
    digests[UnitIndex(SaveUnit::Light)] = DigestArray(terrain.light, MAP_CELLS * LIGHT_CHANNELS, FNV_OFFSET_BASIS);
    digests[UnitIndex(SaveUnit::Objects)] = DigestObjects(objects);
    return digests;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
