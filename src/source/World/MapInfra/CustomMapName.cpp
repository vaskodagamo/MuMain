#include "stdafx.h"

#include "CustomMapName.h"

#include "Core/Text/Utf8.h"
#include "MapNumbers.h"

#include <cstdio>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace World::MapNames
{
namespace
{
constexpr std::string_view UTF8_BYTE_ORDER_MARK = "\xEF\xBB\xBF";
constexpr std::string_view LINE_SPACE = " \t\r\v\f";
// Only the first line is wanted; a name file is a line of text.
constexpr std::size_t READ_LIMIT_BYTES = 4096;
constexpr unsigned char UTF8_CONTINUATION_MASK = 0xC0;
constexpr unsigned char UTF8_CONTINUATION_BITS = 0x80;
constexpr unsigned char FIRST_PRINTABLE = 0x20;
constexpr unsigned char DELETE_CHARACTER = 0x7F;
constexpr const wchar_t* DATA_FOLDER = L"Data";
constexpr const wchar_t* WORLD_FOLDER_PREFIX = L"World";

// The names read so far: a map without a name file is remembered as nullopt, so the
// HUD, which asks every frame, reads each file once.
std::map<int, std::optional<std::wstring>>& Cache()
{
    static std::map<int, std::optional<std::wstring>> cache;
    return cache;
}

std::string_view TrimSpace(std::string_view text)
{
    const std::size_t first = text.find_first_not_of(LINE_SPACE);
    if (first == std::string_view::npos)
        return {};
    const std::size_t last = text.find_last_not_of(LINE_SPACE);
    return text.substr(first, last - first + 1);
}

bool IsContinuationByte(char c)
{
    return (static_cast<unsigned char>(c) & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_BITS;
}

// At most MAX_NAME_BYTES, never ending inside a UTF-8 character.
std::string_view CutToLimit(std::string_view text)
{
    if (text.size() <= MAX_NAME_BYTES)
        return text;
    std::size_t end = MAX_NAME_BYTES;
    while (end > 0 && IsContinuationByte(text[end]))
        --end;
    return TrimSpace(text.substr(0, end));
}

// Control characters would draw as boxes in the HUD.
std::string WithoutControlCharacters(std::string_view text)
{
    std::string clean(text);
    for (char& c : clean)
    {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < FIRST_PRINTABLE || byte == DELETE_CHARACTER)
            c = ' ';
    }
    return clean;
}

std::string ReadNameFile(int map)
{
    const std::filesystem::path file = std::filesystem::path(DATA_FOLDER) /
                                       (WORLD_FOLDER_PREFIX + std::to_wstring(MapNumbers::FolderOf(map))) / NAME_FILE;
    FILE* fp = _wfopen(file.wstring().c_str(), L"rb");
    if (fp == nullptr)
        return {};
    std::string text(READ_LIMIT_BYTES, '\0');
    const std::size_t read = fread(text.data(), 1, text.size(), fp);
    fclose(fp);
    text.resize(read);
    return text;
}
} // namespace

std::string ParseNameFile(std::string_view text)
{
    if (text.substr(0, UTF8_BYTE_ORDER_MARK.size()) == UTF8_BYTE_ORDER_MARK)
        text.remove_prefix(UTF8_BYTE_ORDER_MARK.size());
    while (!text.empty())
    {
        const std::size_t lineEnd = text.find('\n');
        const std::string_view line = TrimSpace(text.substr(0, lineEnd));
        if (!line.empty())
            return WithoutControlCharacters(CutToLimit(line));
        if (lineEnd == std::string_view::npos)
            break;
        text.remove_prefix(lineEnd + 1);
    }
    return {};
}

const wchar_t* Find(int map)
{
    if (map < MapNumbers::FIRST_NEW_MAP || map > MapNumbers::LAST_NEW_MAP)
        return nullptr;

    auto& cache = Cache();
    auto entry = cache.find(map);
    if (entry == cache.end())
    {
        std::wstring name = Core::Text::FromUtf8(ParseNameFile(ReadNameFile(map)));
        std::optional<std::wstring> known;
        if (!name.empty())
            known = std::move(name);
        entry = cache.emplace(map, std::move(known)).first;
    }
    return entry->second ? entry->second->c_str() : nullptr;
}

void Forget(int map)
{
    Cache().erase(map);
}
} // namespace World::MapNames
