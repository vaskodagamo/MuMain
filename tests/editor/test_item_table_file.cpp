#include "App/stdafx.h"

#include <doctest.h>

#include "TempTree.h"

#include "Data/DataHandler/ItemData/ItemDataHandler.h"
#include "Data/DataHandler/ItemData/ItemFileSnapshot.h"
#include "Data/GameData/ItemData/ItemStructs.h"

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

extern ITEM_ATTRIBUTE* ItemAttribute;

namespace fs = std::filesystem;

namespace
{
// The item tables shipped in src/bin/Data. The game loads Data/Local/<lang>/Item_<lang>.bmd;
// Data/Local/Item.bmd is the older table next to them. All are in the legacy layout.
const char* const SHIPPED_TABLES[] = {
    "Local/Eng/item_eng.bmd",
    "Local/Por/item_por.bmd",
    "Local/Spn/item_spn.bmd",
    "Local/Item.bmd",
};

constexpr size_t CHECKSUM_BYTES = sizeof(DWORD);
constexpr int KRIS = 0; // sword 0, the first item of the table
constexpr size_t LEGACY_NAME_BYTES = 29; // char Name[30] with its terminator

// ItemAttribute is allocated by the client at start-up; the test owns one for its run.
class ItemTable
{
public:
    ItemTable() : m_items(std::make_unique<ITEM_ATTRIBUTE[]>(MAX_ITEM))
    {
        m_previous = ItemAttribute;
        ItemAttribute = m_items.get();
    }
    ~ItemTable()
    {
        ItemAttribute = m_previous;
    }
    ItemTable(const ItemTable&) = delete;
    ItemTable& operator=(const ItemTable&) = delete;

private:
    std::unique_ptr<ITEM_ATTRIBUTE[]> m_items;
    ITEM_ATTRIBUTE* m_previous = nullptr;
};

fs::path ShippedTable(const char* dataRelative)
{
    return fs::path(MU_TEST_DATA_DIR) / dataRelative;
}

bool Load(const fs::path& file)
{
    std::wstring name = file.wstring();
    return g_ItemDataHandler.Load(name.data());
}

bool Save(const fs::path& file)
{
    std::wstring name = file.wstring();
    return g_ItemDataHandler.Save(name.data());
}

// Offsets of the bytes where `a` and `b` differ; a size difference counts as one at the end.
std::vector<size_t> DifferingOffsets(const std::string& a, const std::string& b)
{
    std::vector<size_t> offsets;
    const size_t common = std::min(a.size(), b.size());
    for (size_t i = 0; i < common; ++i)
    {
        if (a[i] != b[i])
            offsets.push_back(i);
    }
    if (a.size() != b.size())
        offsets.push_back(common);
    return offsets;
}
} // namespace

TEST_CASE("an item table saved without an edit is byte-identical to the file it was loaded from")
{
    EditorTest::TempTree temp("mu_item_table_roundtrip");
    ItemTable table;

    for (const char* shipped : SHIPPED_TABLES)
    {
        CAPTURE(shipped);
        const fs::path source = ShippedTable(shipped);
        REQUIRE(fs::exists(source));
        REQUIRE(Load(source));
        CHECK(ItemFileSnapshot::RecordSize() == sizeof(ITEM_ATTRIBUTE_FILE_LEGACY));
        CHECK(g_ItemDataHandler.GetMaxNameBytes() == LEGACY_NAME_BYTES);

        const fs::path saved = temp.Root() / source.filename();
        REQUIRE(Save(saved));
        CHECK(DifferingOffsets(EditorTest::ReadText(source), EditorTest::ReadText(saved)).empty());
    }
}

TEST_CASE("a renamed item changes only its own record and the checksum")
{
    EditorTest::TempTree temp("mu_item_table_rename");
    ItemTable table;

    const fs::path source = ShippedTable(SHIPPED_TABLES[0]);
    REQUIRE(Load(source));
    std::wcsncpy(ItemAttribute[KRIS].Name, L"Kris Renamed", MAX_ITEM_NAME - 1);

    const fs::path saved = temp.Root() / source.filename();
    REQUIRE(Save(saved));

    const std::string before = EditorTest::ReadText(source);
    const std::string after = EditorTest::ReadText(saved);
    REQUIRE(after.size() == before.size());
    const size_t recordSize = sizeof(ITEM_ATTRIBUTE_FILE_LEGACY);
    const size_t checksumStart = before.size() - CHECKSUM_BYTES;
    for (const size_t offset : DifferingOffsets(before, after))
    {
        CAPTURE(offset);
        CHECK((offset < (KRIS + 1) * recordSize || offset >= checksumStart));
    }

    REQUIRE(Load(saved));
    CHECK(std::wstring(ItemAttribute[KRIS].Name) == L"Kris Renamed");
}
