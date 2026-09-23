#pragma once

#ifdef _EDITOR

#include <ctime>
#include <set>
#include <string>
#include <vector>

// Names and times of regeneration requests (assets-work/World1/requests/README.md):
// the folder name and id is <YYYY-MM-DD>-<model in lower case>-<slug>, where the
// slug is one to five lower-case words joined by '-', and timestamps are RFC 3339
// with a UTC offset (2026-09-23T10:15:00+02:00).
namespace Editor::Assets
{
// The local date and time of one moment, in the two forms a request uses.
struct Timestamp
{
    std::string date;     // 2026-09-23
    std::string dateTime; // 2026-09-23T10:15:00+02:00
};

// `local` is the broken-down local time, `offsetMinutes` its offset from UTC.
Timestamp FormatTimestamp(const std::tm& local, int offsetMinutes);
Timestamp CurrentTimestamp();

// Slug from the owner's summary: its first words in lower case (letters and
// digits only), leaving out filler words such as "the" or "a". "request" when
// the summary has no usable word.
std::string MakeSlug(const std::string& summary);

// "<date>-<model lower case>-<slug>". A <model>-<slug> that another request
// already uses (on any date; the worker branch name depends only on it) gets a
// number word appended (-2, -3, ...), as does a folder that exists already.
// Words are dropped from the end of the slug (never the number) when the id
// would exceed the 80-character limit, and a single word still too long is cut.
std::string MakeRequestId(const std::string& date, const std::string& model, const std::string& slug,
                          const std::vector<std::string>& existingIds);

// The next free name of a new variant of `model` (Sign01 -> Sign03 when Sign02 is
// taken): the model's family followed by the lowest two-digit number whose name
// (compared in lower case) is not in `takenLower`. Empty when `model` has no
// trailing number.
std::string NextVariantName(const std::string& model, const std::set<std::string>& takenLower);

std::string ToLower(std::string text);
} // namespace Editor::Assets

#endif // _EDITOR
