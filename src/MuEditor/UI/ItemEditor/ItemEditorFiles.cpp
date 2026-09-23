#include "stdafx.h"

#ifdef _EDITOR

#include "ItemEditorFiles.h"

#include <string>

namespace fs = std::filesystem;

namespace Editor::Files
{
namespace
{
constexpr const wchar_t* LOCAL_FOLDER = L"Local";
constexpr const wchar_t* ITEM_TABLE_PREFIX = L"Item_";
constexpr const wchar_t* ITEM_TABLE_EXTENSION = L".bmd";
constexpr const wchar_t* LEGACY_EXPORT_NAME = L"Item_S6E3.bmd";
constexpr const wchar_t* CSV_EXPORT_NAME = L"Item.csv";

fs::path InLanguageDir(const std::wstring& fileName)
{
    return OnDiskSpelling(LanguageDir() / fileName);
}
} // namespace

fs::path LanguageDir()
{
    return OnDiskSpelling(DataDir() / LOCAL_FOLDER / g_strSelectedML);
}

fs::path ItemTableFile()
{
    return InLanguageDir(ITEM_TABLE_PREFIX + g_strSelectedML + ITEM_TABLE_EXTENSION);
}

fs::path ItemLegacyExportFile()
{
    return InLanguageDir(LEGACY_EXPORT_NAME);
}

fs::path ItemCsvExportFile()
{
    return InLanguageDir(CSV_EXPORT_NAME);
}
} // namespace Editor::Files

#endif // _EDITOR
