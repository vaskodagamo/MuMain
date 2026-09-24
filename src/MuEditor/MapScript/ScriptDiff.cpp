#include "ScriptDiff.h"

#ifdef _EDITOR

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace Editor::MapScript
{
namespace
{
constexpr int TILES = Editor::MapInspect::MAP_TILES;
constexpr std::size_t LIGHT_CHANNELS = 3;

void Include(LayerChange& change, int x, int y)
{
    if (change.cells == 0)
        change.area = CellRect{x, y, x, y};
    change.area.minX = std::min(change.area.minX, x);
    change.area.minY = std::min(change.area.minY, y);
    change.area.maxX = std::max(change.area.maxX, x);
    change.area.maxY = std::max(change.area.maxY, y);
    ++change.cells;
}

template <typename T>
bool CellDiffers(const std::vector<T>& a, const std::vector<T>& b, std::size_t cell, std::size_t width)
{
    return std::memcmp(&a[cell * width], &b[cell * width], sizeof(T) * width) != 0;
}

// Calls differs(cell) for every cell and adds the ones it reports to `change`.
template <typename Differs> LayerChange CompareCells(Differs differs)
{
    LayerChange change;
    for (int y = 0; y < TILES; ++y)
    {
        for (int x = 0; x < TILES; ++x)
        {
            if (differs(Editor::MapInspect::CellIndex(x, y)))
                Include(change, x, y);
        }
    }
    return change;
}

void DiffTerrain(const MapTerrain& before, const MapTerrain& after, MapChanges& changes)
{
    changes.height = CompareCells([&](std::size_t cell) { return CellDiffers(before.height, after.height, cell, 1); });
    changes.texture1 =
        CompareCells([&](std::size_t cell) { return CellDiffers(before.baseTiles, after.baseTiles, cell, 1); });
    changes.texture2 = CompareCells(
        [&](std::size_t cell)
        {
            return CellDiffers(before.overlayTiles, after.overlayTiles, cell, 1) ||
                   CellDiffers(before.overlayAlpha, after.overlayAlpha, cell, 1);
        });
    changes.attribute =
        CompareCells([&](std::size_t cell) { return CellDiffers(before.attribute, after.attribute, cell, 1); });
    changes.light =
        CompareCells([&](std::size_t cell) { return CellDiffers(before.light, after.light, cell, LIGHT_CHANNELS); });
}

void DiffObjects(const MapState& before, const MapState& after, MapChanges& changes)
{
    std::unordered_map<int, const MapObject*> kept;
    for (const MapObject& object : after.objects)
    {
        if (object.key != NEW_OBJECT)
            kept.emplace(object.key, &object);
    }
    for (const MapObject& object : before.objects)
    {
        if (kept.count(object.key) == 0)
            changes.objects.push_back({ObjectChangeKind::Removed, object.key, object.id, object.state, std::nullopt});
    }
    std::unordered_map<int, const MapObject*> original;
    for (const MapObject& object : before.objects)
        original.emplace(object.key, &object);
    for (const MapObject& object : after.objects)
    {
        const auto found = object.key == NEW_OBJECT ? original.end() : original.find(object.key);
        if (found != original.end() && !(found->second->state == object.state))
            changes.objects.push_back(
                {ObjectChangeKind::Changed, object.key, object.id, found->second->state, object.state});
    }
    for (const MapObject& object : after.objects)
    {
        if (object.key == NEW_OBJECT)
            changes.objects.push_back({ObjectChangeKind::Added, NEW_OBJECT, object.id, std::nullopt, object.state});
    }
}
} // namespace

bool MapChanges::Any() const
{
    return height.Changed() || texture1.Changed() || texture2.Changed() || attribute.Changed() || light.Changed() ||
           !objects.empty();
}

int MapChanges::Count(ObjectChangeKind kind) const
{
    return static_cast<int>(
        std::count_if(objects.begin(), objects.end(), [kind](const ObjectDiff& diff) { return diff.kind == kind; }));
}

MapChanges Diff(const MapState& before, const MapState& after)
{
    MapChanges changes;
    DiffTerrain(before.terrain, after.terrain, changes);
    DiffObjects(before, after, changes);
    return changes;
}
} // namespace Editor::MapScript

#endif // _EDITOR
