#include "TerrainHeightFile.h"

#ifdef _EDITOR

namespace Editor::HeightMap
{
bool DecodeOzb(const std::vector<std::uint8_t>& file, float factor, float* heights, std::string& error)
{
    if (file.size() < OZB_BYTES)
    {
        error = "the height file is " + std::to_string(file.size()) + " bytes; one needs " + std::to_string(OZB_BYTES);
        return false;
    }
    const std::uint8_t* corners = file.data() + OZB_PREFIX_BYTES + BMP_HEADER_BYTES;
    const std::size_t count = static_cast<std::size_t>(HEIGHT_MAP_SIZE) * HEIGHT_MAP_SIZE;
    for (std::size_t i = 0; i < count; ++i)
        heights[i] = static_cast<float>(corners[i]) * factor;
    return true;
}
} // namespace Editor::HeightMap

#endif // _EDITOR
