#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Text helpers the world editor shares between its file-only code (the Assets
// folder, also compiled into the editor tests) and its UI. No ImGui, no engine.
namespace Editor::Text
{
// The path as UTF-8 text, for ImGui, the logs and the files a request writes.
// Never path::string(): on Windows it converts to the ANSI code page and throws
// for a character that code page lacks.
std::string PathToUtf8(const std::filesystem::path& path);

// The same with '/' between the folders on every platform, as git and the
// asset catalog write repository paths.
std::string GenericPathToUtf8(const std::filesystem::path& path);

// UTF-8 text (a catalog path, a texture name, a git ref) as a path.
std::filesystem::path Utf8Path(std::string_view text);

// 'A'..'Z' to 'a'..'z'; every other byte unchanged (UTF-8 stays intact).
char LowerAscii(char c);
bool EqualIgnoringCase(std::string_view a, std::string_view b);
// `needle` occurs in `text`, ignoring ASCII case; an empty needle always does.
bool ContainsIgnoringCase(std::string_view text, std::string_view needle);

// `text` in lower case for searching: A-Z and the capitals of Latin-1, Latin
// Extended-A, Greek and Cyrillic (the letters item names in the shipped
// languages use) become their small letters; everything else, including bytes
// that are not UTF-8, stays as it is. Search a folded needle in a folded text.
std::string FoldCase(std::string_view text);

// The items with `separator` between them; empty for no items.
std::string Join(const std::vector<std::string>& items, std::string_view separator);

// `text` without the leading and trailing white space Python's str.strip()
// removes, which the request validator applies: ASCII spaces and control
// separators, and Unicode spaces such as the no-break space (U+00A0) that text
// pasted from a web page often holds.
std::string Trim(std::string_view text);

// One item per line of `text`, trimmed; lines with nothing else are left out.
std::vector<std::string> NonEmptyLines(std::string_view text);
} // namespace Editor::Text

#endif // _EDITOR
