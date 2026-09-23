#pragma once

#ifdef _EDITOR

#include "Core/EditorFiles.h"

#include <filesystem>

// The Item Editor's files in the client data tree, for the language the client
// runs in (config.ini LanguageSelection, e.g. Eng); the save, mirror and read
// helpers every editor shares are in Core/EditorFiles.h. Each path is spelled
// as the file on disk when it exists (the game asks for Item_Eng.bmd, git
// tracks item_eng.bmd), so a save names the file git tracks.
namespace Editor::Files
{
std::filesystem::path LanguageDir(); // Data/Local/{lang}

// The item table the game loads and the Item Editor saves.
std::filesystem::path ItemTableFile(); // Data/Local/{lang}/Item_{lang}.bmd

// Exports for other tools; the game does not read them.
std::filesystem::path ItemLegacyExportFile(); // Data/Local/{lang}/Item_S6E3.bmd
std::filesystem::path ItemCsvExportFile();    // Data/Local/{lang}/Item.csv
} // namespace Editor::Files

#endif // _EDITOR
