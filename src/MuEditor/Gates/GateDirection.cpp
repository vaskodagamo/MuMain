#include "GateDirection.h"

#ifdef _EDITOR

#include "GateRecord.h"

#include <array>
#include <cctype>

namespace Editor::Gates
{
namespace
{
constexpr std::array<const char*, LAST_DIRECTION + 1> NAMES = {
    "undefined", "west", "southwest", "south", "southeast", "east", "northeast", "north", "northwest"};
constexpr const char* UNKNOWN_NAME = "?";

// Lower case without '-', '_' and spaces: "North-West" -> "northwest".
std::string Normalized(const std::string& name)
{
    std::string normalized;
    for (const char c : name)
    {
        if (c == '-' || c == '_' || c == ' ')
            continue;
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return normalized;
}

bool IsSmallNumber(const std::string& text)
{
    return text.size() == 1 && std::isdigit(static_cast<unsigned char>(text[0]));
}
} // namespace

const char* DirectionName(int direction)
{
    if (direction < 0 || direction > LAST_DIRECTION)
        return UNKNOWN_NAME;
    return NAMES[static_cast<std::size_t>(direction)];
}

bool DirectionFromName(const std::string& name, int& direction)
{
    const std::string normalized = Normalized(name);
    if (IsSmallNumber(normalized) && normalized[0] - '0' <= LAST_DIRECTION)
    {
        direction = normalized[0] - '0';
        return true;
    }
    for (int candidate = 0; candidate <= LAST_DIRECTION; ++candidate)
    {
        if (normalized == NAMES[static_cast<std::size_t>(candidate)])
        {
            direction = candidate;
            return true;
        }
    }
    return false;
}

std::string DirectionNames()
{
    std::string names;
    for (const char* name : NAMES)
        names += (names.empty() ? "" : ", ") + std::string(name);
    return names;
}
} // namespace Editor::Gates

#endif // _EDITOR
