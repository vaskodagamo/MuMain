#include "stdafx.h"

#ifdef _EDITOR

#include "ItemDataSaver.h"
#include "ItemFileSnapshot.h"
#include "Data/GameData/ItemData/ItemStructs.h"
#include "Core/Globals/_struct.h"
#include "Core/Globals/_define.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Data/Translation/MultiLanguage.h"
#include "GameLogic/Events/CSChaosCastle.h"
#include <sstream>
#include <memory>

#include "Data/DataHandler/ChangeTracker.h"
#include "Data/DataHandler/CommonDataSaver.h"
#include "Data/DataHandler/FieldMetadata.h"
#include "Data/DataHandler/ItemData/ItemComparisonMetadata.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "Core/Utilities/StringUtils.h"

// External references
extern ITEM_ATTRIBUTE* ItemAttribute;

// Comparison function - uses field metadata (NO MACROS!)
static void CompareItems(const ITEM_ATTRIBUTE& oldItem, const ITEM_ATTRIBUTE& newItem,
                        std::stringstream& changes, bool& changed)
{
    // Compare Name field (wide string)
    if (wcscmp(oldItem.Name, newItem.Name) != 0)
    {
        char oldUtf8[256];
        char newUtf8[256];
        WideCharToMultiByte(CP_UTF8, 0, oldItem.Name, -1, oldUtf8, sizeof(oldUtf8), NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, newItem.Name, -1, newUtf8, sizeof(newUtf8), NULL, NULL);

        changes << "  Name: \"" << oldUtf8 << "\" -> \"" << newUtf8 << "\"\n";
        changed = true;
    }

    // Compare all simple fields - just a loop, no macros!
    CompareAllFieldsByMetadata(oldItem, newItem,
                               ItemComparisonMetadata::SIMPLE_FIELDS,
                               changes, changed);

    // Compare all array fields - another simple loop!
    CompareAllFieldsByMetadata(oldItem, newItem,
                               ItemComparisonMetadata::ARRAY_FIELDS,
                               changes, changed);
}

// When item `index` is unchanged since the load, copies its loaded bytes over
// `record`: decoding them again gives the item exactly as the loader did.
template <typename TFile>
static void KeepLoadedBytesIfUnchanged(size_t index, const ITEM_ATTRIBUTE& current, BYTE* record)
{
    const BYTE* loaded = ItemFileSnapshot::Record(index);
    if (loaded == nullptr || ItemFileSnapshot::RecordSize() != sizeof(TFile))
        return;

    TFile loadedRecord;
    memcpy(&loadedRecord, loaded, sizeof(loadedRecord));
    ITEM_ATTRIBUTE loadedItem{};
    CopyItemAttributeFromSource(loadedItem, loadedRecord);

    std::stringstream ignoredChanges;
    bool changed = false;
    CompareItems(loadedItem, current, ignoredChanges, changed);
    if (!changed)
        memcpy(record, loaded, sizeof(TFile));
}

bool ItemDataSaver::Save(wchar_t* fileName, std::string* outChangeLog)
{
    // A file loaded in the legacy layout (30-byte names) is written back in it, so
    // a save never converts the game's file behind the user's back.
    if (ItemFileSnapshot::RecordSize() == sizeof(ITEM_ATTRIBUTE_FILE_LEGACY))
        return SaveAs<ITEM_ATTRIBUTE_FILE_LEGACY>(fileName, outChangeLog);
    return SaveAs<ITEM_ATTRIBUTE_FILE>(fileName, outChangeLog);
}

size_t ItemDataSaver::MaxNameBytes()
{
    if (ItemFileSnapshot::RecordSize() == sizeof(ITEM_ATTRIBUTE_FILE_LEGACY))
        return sizeof(ITEM_ATTRIBUTE_FILE_LEGACY::Name) - 1;
    return sizeof(ITEM_ATTRIBUTE_FILE::Name) - 1;
}

template <typename TFile>
bool ItemDataSaver::SaveAs(wchar_t* fileName, std::string* outChangeLog)
{
    // Create standard save config with item-specific parameters
    auto config = CreateStandardSaveConfig<ITEM_ATTRIBUTE, TFile>(
        fileName,
        MAX_ITEM,
        ItemAttribute,
        [](TFile& dest, const ITEM_ATTRIBUTE& src) {
            CopyItemAttributeToDestination(dest, src);
        },
        [](ITEM_ATTRIBUTE& dest, const TFile& src) {
            CopyItemAttributeFromSource(dest, src);
        },
        CompareItems,
        [](int index, const ITEM_ATTRIBUTE& item) {
            return ChangeTracker::GetNameUtf8(index, item, MAX_ITEM_NAME);
        },
        0xE2F1,  // Item-specific checksum key
        outChangeLog
    );
    config.keepLoadedBytes = KeepLoadedBytesIfUnchanged<TFile>;

    // Add legacy format support for backwards compatibility with old Item.bmd files
    config.legacyFileStructSize = sizeof(ITEM_ATTRIBUTE_FILE_LEGACY);
    config.convertFromFileLegacy = [](ITEM_ATTRIBUTE& dest, BYTE* buffer, size_t size) {
        ITEM_ATTRIBUTE_FILE_LEGACY legacyStruct;
        memcpy(&legacyStruct, buffer, sizeof(legacyStruct));
        CopyItemAttributeFromSource(dest, legacyStruct);
    };

    // Use generic save method
    bool result = CommonDataSaver::SaveData(config);

    // Adjust change log header
    if (result && outChangeLog && !outChangeLog->empty())
    {
        size_t pos = outChangeLog->find("=== Changes");
        if (pos != std::string::npos)
        {
            outChangeLog->replace(pos, 11, "=== Item Changes");
        }
    }

    return result;
}

#endif // _EDITOR
