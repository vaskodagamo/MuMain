#include "SavedMapState.h"

#ifdef _EDITOR

namespace Editor::MapInspect
{
void SavedMapState::Reset(const MapDigests& loaded)
{
    m_saved = loaded;
    m_known = true;
}

void SavedMapState::MarkSaved(SaveUnit unit, std::uint64_t digest)
{
    m_saved[static_cast<std::size_t>(unit)] = digest;
}

std::array<bool, SAVE_UNIT_COUNT> SavedMapState::Unsaved(const MapDigests& current) const
{
    std::array<bool, SAVE_UNIT_COUNT> unsaved{};
    if (!m_known)
        return unsaved;
    for (std::size_t unit = 0; unit < SAVE_UNIT_COUNT; ++unit)
        unsaved[unit] = current[unit] != m_saved[unit];
    return unsaved;
}
} // namespace Editor::MapInspect

#endif // _EDITOR
