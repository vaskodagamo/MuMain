#include "stdafx.h"

#ifdef _EDITOR

#include "TextSearch.h"

#include "Assets/EditorText.h"
#include "Data/GameData/Common/LocalizedString.h"

#include <algorithm>
#include <charconv>

namespace MuEditor::TextSearch
{
// Not std::tolower: it follows the process locale, which the client takes from
// the system; with a Windows code page it changes bytes of UTF-8 letters.
// Editor::Text::FoldCase folds the letters item names use (A to Z, Latin-1, Latin
// Extended-A, Greek, Cyrillic) whatever the locale.
bool ContainsIgnoringCase(std::string_view text, std::string_view search)
{
    return Editor::Text::FoldCase(text).find(Editor::Text::FoldCase(search)) != std::string::npos;
}

bool AnyNameContains(const Data::LocalizedString& names, std::string_view search)
{
    if (ContainsIgnoringCase(names.GetNeutral(), search))
    {
        return true;
    }
    const auto& translations = names.GetTranslations();
    return std::any_of(translations.begin(), translations.end(),
                       [&](const auto& translation) { return ContainsIgnoringCase(translation.second, search); });
}

bool ReadWholeNumber(std::string_view text, int& value)
{
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
} // namespace MuEditor::TextSearch

#endif // _EDITOR
