#pragma once
#include "Data/GameData/ItemData/ItemFieldMetadata.h"

#ifdef _EDITOR

#include "Core/Globals/_struct.h"
#include <string>

// Handles Save/Export action buttons for the Item Editor
// Uses metadata-driven approach for automatic export generation
class CItemEditorActions
{
public:
    static void RenderSaveButton();
    static void RenderExportS6E3Button();
    static void RenderExportCSVButton();

    // Render all action buttons in a row, and under them the paths the last
    // save or export wrote
    static void RenderAllButtons();

    // Export item data to CSV format (metadata-driven)
    static std::string ExportItemToCSV(int itemIndex, ITEM_ATTRIBUTE& item);

    // Export item data to readable format (key=value pairs, metadata-driven)
    static std::string ExportItemToReadable(int itemIndex, ITEM_ATTRIBUTE& item);

    // Export both formats combined (readable + CSV)
    static std::string ExportItemCombined(int itemIndex, ITEM_ATTRIBUTE& item);

    // Get CSV header row (metadata-driven)
    static std::string GetCSVHeader();

private:
    // Save writes Data/Local/<lang>/Item_<lang>.bmd and mirrors it into the
    // repository (Core/EditorFiles.h); the exports stay next to it and are copied
    // to <repo>/out/editor-exports.
    static void SaveItemTable();
    static void ExportLegacyTable();
    static void ExportCsv();
    static void RenderStatus();

    // What the last save or export wrote, shown under the buttons.
    static std::string s_status;

    // Helper to convert item name to UTF-8
    static void ConvertItemName(char* outBuffer, size_t bufferSize, const wchar_t* name);

    // Get field value as string (descriptor-driven)
    static std::string GetFieldValueAsString(const ITEM_ATTRIBUTE &item, const ItemFieldDescriptor &desc);
};

#endif // _EDITOR
