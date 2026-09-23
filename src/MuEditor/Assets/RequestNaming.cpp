#include "RequestNaming.h"

#ifdef _EDITOR

#include "EditorText.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string_view>

namespace Editor::Assets
{
namespace
{
constexpr std::size_t DATE_CHARS = 10; // YYYY-MM-DD
constexpr std::size_t MAX_ID_CHARS = 80;
constexpr std::size_t MAX_SLUG_WORDS = 5;
// Words taken from the summary; one word stays free for a -2/-3 suffix.
constexpr std::size_t SUMMARY_SLUG_WORDS = 4;
constexpr const char* FALLBACK_SLUG = "request";
constexpr char WORD_SEPARATOR = '-';
constexpr int FIRST_DUPLICATE_NUMBER = 2;
constexpr int MAX_DUPLICATE_NUMBER = 999;
constexpr int FIRST_VARIANT_NUMBER = 1;
constexpr int MAX_VARIANT_NUMBER = 999;
constexpr int TM_YEAR_BASE = 1900;
constexpr int MINUTES_PER_HOUR = 60;
constexpr int MINUTES_PER_DAY = 24 * MINUTES_PER_HOUR;
constexpr std::size_t TIMESTAMP_CHARS = 32;
constexpr std::size_t NUMBER_CHARS = 16;

// Left out of slugs: they make ids longer without telling requests apart.
constexpr std::string_view FILLER_WORDS[] = {
    "a",  "an", "and", "as", "at",   "be",  "by",   "for", "from", "in",   "is",
    "it", "of", "on",  "or", "that", "the", "this", "to",  "too",  "with", "its",
};

bool IsWordChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

std::vector<std::string> Words(const std::string& text)
{
    std::vector<std::string> words;
    std::string word;
    for (char c : ToLower(text) + ' ')
    {
        if (IsWordChar(c))
        {
            word.push_back(c);
            continue;
        }
        if (!word.empty())
            words.push_back(word);
        word.clear();
    }
    return words;
}

bool IsFiller(const std::string& word)
{
    return std::find(std::begin(FILLER_WORDS), std::end(FILLER_WORDS), word) != std::end(FILLER_WORDS);
}

std::string Join(const std::vector<std::string>& words)
{
    return Editor::Text::Join(words, std::string(1, WORD_SEPARATOR));
}

std::vector<std::string> FirstWords(const std::vector<std::string>& words, std::size_t count)
{
    return std::vector<std::string>(words.begin(),
                                    words.begin() + static_cast<std::ptrdiff_t>(std::min(count, words.size())));
}

bool IsTaken(const std::string& id, const std::vector<std::string>& existingIds)
{
    // Branch and worktree names depend only on <model>-<slug>, so any date collides.
    const std::string_view suffix = std::string_view(id).substr(DATE_CHARS);
    return std::any_of(
        existingIds.begin(), existingIds.end(), [&](const std::string& other)
        { return other == id || (other.size() > DATE_CHARS && std::string_view(other).substr(DATE_CHARS) == suffix); });
}

// "<date>-<model>-<slug words>[-<number>]" within MAX_ID_CHARS: slug words are
// dropped from the end, never the number, and a single word that is still too
// long is cut. Only a model name too long for any slug gives a longer id.
std::string ComposeId(const std::string& date, const std::string& model, std::vector<std::string> words,
                      const std::string& number)
{
    const std::string prefix = date + WORD_SEPARATOR + ToLower(model) + WORD_SEPARATOR;
    const std::string suffix = number.empty() ? std::string() : WORD_SEPARATOR + number;
    const auto compose = [&] { return prefix + Join(words) + suffix; };
    while (compose().size() > MAX_ID_CHARS && words.size() > 1)
        words.pop_back();
    const std::size_t fixedChars = prefix.size() + suffix.size();
    if (compose().size() > MAX_ID_CHARS && fixedChars < MAX_ID_CHARS)
        words.front().resize(MAX_ID_CHARS - fixedChars);
    return compose();
}

void LocalAndUtc(std::time_t now, std::tm& local, std::tm& utc)
{
#ifdef _WIN32
    localtime_s(&local, &now);
    gmtime_s(&utc, &now);
#else
    localtime_r(&now, &local);
    gmtime_r(&now, &utc);
#endif
}

// Local time minus UTC, from the two broken-down forms of the same moment.
int OffsetMinutes(const std::tm& local, const std::tm& utc)
{
    int dayDifference = local.tm_yday - utc.tm_yday;
    if (local.tm_year != utc.tm_year)
        dayDifference = local.tm_year > utc.tm_year ? 1 : -1;
    return dayDifference * MINUTES_PER_DAY + (local.tm_hour - utc.tm_hour) * MINUTES_PER_HOUR +
           (local.tm_min - utc.tm_min);
}
} // namespace

std::string ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), Editor::Text::LowerAscii);
    return text;
}

Timestamp FormatTimestamp(const std::tm& local, int offsetMinutes)
{
    char date[TIMESTAMP_CHARS];
    std::snprintf(date, sizeof(date), "%04d-%02d-%02d", local.tm_year + TM_YEAR_BASE, local.tm_mon + 1, local.tm_mday);
    const char sign = offsetMinutes < 0 ? '-' : '+';
    const int magnitude = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;
    char dateTime[TIMESTAMP_CHARS];
    std::snprintf(dateTime, sizeof(dateTime), "%sT%02d:%02d:%02d%c%02d:%02d", date, local.tm_hour, local.tm_min,
                  local.tm_sec, sign, magnitude / MINUTES_PER_HOUR, magnitude % MINUTES_PER_HOUR);
    return {date, dateTime};
}

Timestamp CurrentTimestamp()
{
    std::tm local{};
    std::tm utc{};
    LocalAndUtc(std::time(nullptr), local, utc);
    return FormatTimestamp(local, OffsetMinutes(local, utc));
}

std::string MakeSlug(const std::string& summary)
{
    const std::vector<std::string> words = Words(summary);
    std::vector<std::string> content;
    std::copy_if(words.begin(), words.end(), std::back_inserter(content),
                 [](const std::string& w) { return !IsFiller(w); });
    std::vector<std::string> chosen = FirstWords(content.empty() ? words : content, SUMMARY_SLUG_WORDS);
    return chosen.empty() ? FALLBACK_SLUG : Join(chosen);
}

std::string MakeRequestId(const std::string& date, const std::string& model, const std::string& slug,
                          const std::vector<std::string>& existingIds)
{
    std::vector<std::string> words = FirstWords(Words(slug), MAX_SLUG_WORDS);
    if (words.empty())
        words.push_back(FALLBACK_SLUG);
    const std::string id = ComposeId(date, model, words, {});
    if (!IsTaken(id, existingIds))
        return id;

    if (words.size() >= MAX_SLUG_WORDS)
        words.pop_back(); // room for the number word
    for (int number = FIRST_DUPLICATE_NUMBER; number <= MAX_DUPLICATE_NUMBER; ++number)
    {
        const std::string candidate = ComposeId(date, model, words, std::to_string(number));
        if (!IsTaken(candidate, existingIds))
            return candidate;
    }
    return id; // a thousand requests with one slug: let the validator report it
}

std::string NextVariantName(const std::string& model, const std::set<std::string>& takenLower)
{
    const auto firstDigit = std::find_if(model.rbegin(), model.rend(), [](char c) { return c < '0' || c > '9'; });
    const std::string family(model.begin(), firstDigit.base());
    if (family.empty() || family.size() == model.size())
        return {};
    for (int number = FIRST_VARIANT_NUMBER; number <= MAX_VARIANT_NUMBER; ++number)
    {
        char digits[NUMBER_CHARS];
        std::snprintf(digits, sizeof(digits), "%02d", number);
        const std::string candidate = family + digits;
        if (!takenLower.contains(ToLower(candidate)))
            return candidate;
    }
    return {};
}
} // namespace Editor::Assets

#endif // _EDITOR
