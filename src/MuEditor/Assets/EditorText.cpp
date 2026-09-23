#include "EditorText.h"

#ifdef _EDITOR

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

namespace Editor::Text
{
namespace
{
struct CodePoint
{
    char32_t value;
    std::size_t bytes;
};

struct CodePointRange
{
    char32_t first;
    char32_t last;
};

// What Python's str.isspace() calls white space.
constexpr CodePointRange PYTHON_WHITESPACE[] = {
    {0x09, 0x0D},     {0x1C, 0x20},     {0x85, 0x85},     {0xA0, 0xA0},     {0x1680, 0x1680},
    {0x2000, 0x200A}, {0x2028, 0x2029}, {0x202F, 0x202F}, {0x205F, 0x205F}, {0x3000, 0x3000},
};

// UTF-8 lead bytes: the bits that mark the sequence length, and the value bits.
struct LeadByte
{
    unsigned char mask;
    unsigned char pattern;
    std::size_t bytes;
};
constexpr LeadByte LEAD_BYTES[] = {
    {0x80, 0x00, 1},
    {0xE0, 0xC0, 2},
    {0xF0, 0xE0, 3},
    {0xF8, 0xF0, 4},
};
constexpr unsigned char CONTINUATION_MASK = 0xC0;
constexpr unsigned char CONTINUATION_PATTERN = 0x80;
constexpr unsigned char CONTINUATION_VALUE_BITS = 0x3F;
constexpr int CONTINUATION_SHIFT = 6;

// The smallest code point each sequence length may carry (shorter forms are overlong),
// indexed by the length in bytes.
constexpr char32_t SHORTEST_FORM_MIN[] = {0, 0, 0x80, 0x800, 0x10000};
constexpr char32_t SURROGATE_FIRST = 0xD800;
constexpr char32_t SURROGATE_LAST = 0xDFFF;
constexpr char32_t LAST_CODE_POINT = 0x10FFFF;
constexpr const char* HEX_DIGITS = "0123456789ABCDEF";
constexpr int HIGH_NIBBLE_SHIFT = 4;
constexpr unsigned char LOW_NIBBLE_MASK = 0x0F;
constexpr char ESCAPE_MARK = '%';
constexpr unsigned char LAST_ASCII = 0x7F;

bool IsContinuation(char c)
{
    return (static_cast<unsigned char>(c) & CONTINUATION_MASK) == CONTINUATION_PATTERN;
}

// The code point whose UTF-8 sequence starts at text[pos]; nullopt when none does.
std::optional<CodePoint> DecodeAt(std::string_view text, std::size_t pos)
{
    const auto lead = static_cast<unsigned char>(text[pos]);
    const auto kind = std::find_if(std::begin(LEAD_BYTES), std::end(LEAD_BYTES),
                                   [lead](const LeadByte& byte) { return (lead & byte.mask) == byte.pattern; });
    if (kind == std::end(LEAD_BYTES) || pos + kind->bytes > text.size())
        return std::nullopt;

    char32_t value = lead & static_cast<unsigned char>(~kind->mask);
    for (std::size_t i = 1; i < kind->bytes; ++i)
    {
        if (!IsContinuation(text[pos + i]))
            return std::nullopt;
        value = (value << CONTINUATION_SHIFT) | (static_cast<unsigned char>(text[pos + i]) & CONTINUATION_VALUE_BITS);
    }
    return CodePoint{value, kind->bytes};
}

// A shortest-form sequence of a Unicode scalar value: what strict UTF-8 readers accept.
bool IsWellFormed(const CodePoint& codePoint)
{
    const bool shortest = codePoint.value >= SHORTEST_FORM_MIN[codePoint.bytes];
    const bool surrogate = codePoint.value >= SURROGATE_FIRST && codePoint.value <= SURROGATE_LAST;
    return shortest && !surrogate && codePoint.value <= LAST_CODE_POINT;
}

bool IsAscii(char c)
{
    return static_cast<unsigned char>(c) <= LAST_ASCII;
}

std::string EscapedByte(char c)
{
    const auto byte = static_cast<unsigned char>(c);
    return {ESCAPE_MARK, HEX_DIGITS[byte >> HIGH_NIBBLE_SHIFT], HEX_DIGITS[byte & LOW_NIBBLE_MASK]};
}

bool IsWhitespace(const std::optional<CodePoint>& codePoint)
{
    if (!codePoint)
        return false;
    return std::any_of(std::begin(PYTHON_WHITESPACE), std::end(PYTHON_WHITESPACE), [&](const CodePointRange& range)
                       { return codePoint->value >= range.first && codePoint->value <= range.last; });
}

// Where the trimmed text starts.
std::size_t SkipLeadingWhitespace(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size())
    {
        const std::optional<CodePoint> codePoint = DecodeAt(text, begin);
        if (!IsWhitespace(codePoint))
            break;
        begin += codePoint->bytes;
    }
    return begin;
}

// Where the trimmed text ends, not before `begin`.
std::size_t SkipTrailingWhitespace(std::string_view text, std::size_t begin)
{
    std::size_t end = text.size();
    while (end > begin)
    {
        std::size_t start = end - 1;
        while (start > begin && IsContinuation(text[start]))
            --start;
        const std::optional<CodePoint> codePoint = DecodeAt(text, start);
        if (!IsWhitespace(codePoint) || start + codePoint->bytes != end)
            break;
        end = start;
    }
    return end;
}
} // namespace

std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

std::string GenericPathToUtf8(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.generic_u8string();
    return std::string(utf8.begin(), utf8.end());
}

std::filesystem::path Utf8Path(std::string_view text)
{
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

char LowerAscii(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool EqualIgnoringCase(std::string_view a, std::string_view b)
{
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return LowerAscii(x) == LowerAscii(y); });
}

bool ContainsIgnoringCase(std::string_view text, std::string_view needle)
{
    const auto found = std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                                   [](char a, char b) { return LowerAscii(a) == LowerAscii(b); });
    return found != text.end() || needle.empty();
}

std::string Join(const std::vector<std::string>& items, std::string_view separator)
{
    std::string text;
    for (const std::string& item : items)
    {
        if (!text.empty())
            text += separator;
        text += item;
    }
    return text;
}

std::string Trim(std::string_view text)
{
    const std::size_t begin = SkipLeadingWhitespace(text);
    const std::size_t end = SkipTrailingWhitespace(text, begin);
    return std::string(text.substr(begin, end - begin));
}

std::vector<std::string> NonEmptyLines(std::string_view text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size())
    {
        const std::size_t newline = std::min(text.find('\n', start), text.size());
        std::string line = Trim(text.substr(start, newline - start));
        if (!line.empty())
            lines.push_back(std::move(line));
        start = newline + 1;
    }
    return lines;
}

bool IsValidUtf8(std::string_view text)
{
    std::size_t pos = 0;
    while (pos < text.size())
    {
        const std::optional<CodePoint> codePoint = DecodeAt(text, pos);
        if (!codePoint || !IsWellFormed(*codePoint))
            return false;
        pos += codePoint->bytes;
    }
    return true;
}

std::string ValidUtf8(std::string_view text)
{
    if (IsValidUtf8(text))
        return std::string(text);
    // Not UTF-8, so a legacy code page: every byte outside ASCII is escaped, also the ones
    // that happen to form a UTF-8 sequence with their neighbours.
    std::string escaped;
    escaped.reserve(text.size());
    for (const char c : text)
        escaped += IsAscii(c) ? std::string(1, c) : EscapedByte(c);
    return escaped;
}
} // namespace Editor::Text

#endif // _EDITOR
