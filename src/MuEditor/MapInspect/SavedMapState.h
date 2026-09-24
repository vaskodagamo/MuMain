#pragma once

#ifdef _EDITOR

#include "MapDigest.h"

#include <array>

namespace Editor::MapInspect
{
// What the loaded map's files hold, as digests (MapDigest.h): taken when the map
// loads and updated by every save, so edits that were not saved yet show up as a
// difference. An edit that is undone again is no difference.
class SavedMapState
{
public:
    // The map as it was loaded.
    void Reset(const MapDigests& loaded);
    // `unit` was saved while its digest was `digest`.
    void MarkSaved(SaveUnit unit, std::uint64_t digest);

    // False before the first Reset.
    bool IsKnown() const
    {
        return m_known;
    }

    // Per unit: true when `current` differs from the last load or save.
    std::array<bool, SAVE_UNIT_COUNT> Unsaved(const MapDigests& current) const;

private:
    MapDigests m_saved{};
    bool m_known = false;
};
} // namespace Editor::MapInspect

#endif // _EDITOR
