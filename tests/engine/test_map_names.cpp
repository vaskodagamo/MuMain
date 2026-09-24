#include <doctest.h>

#include "World/MapInfra/CustomMapName.h"
#include "World/MapInfra/MapNumbers.h"

#include <string>

using World::MapNames::MAX_NAME_BYTES;
using World::MapNames::ParseNameFile;

TEST_CASE("Map names: the first line with text, trimmed")
{
    CHECK(ParseNameFile("Lorencia Outskirts\n") == "Lorencia Outskirts");
    CHECK(ParseNameFile("\r\n  \n\t Lorencia Outskirts \r\nsecond line") == "Lorencia Outskirts");
    CHECK(ParseNameFile("\xEF\xBB\xBFOutskirts") == "Outskirts");
    CHECK(ParseNameFile("").empty());
    CHECK(ParseNameFile(" \n\t\r\n").empty());
}

TEST_CASE("Map names: control characters and long names")
{
    CHECK(ParseNameFile("Out\x01skirts") == "Out skirts");
    const std::string longName(MAX_NAME_BYTES + 10, 'x');
    CHECK(ParseNameFile(longName).size() == MAX_NAME_BYTES);

    // A two-byte character across the limit is left out whole, not cut in half.
    std::string umlauts(MAX_NAME_BYTES - 1, 'a');
    umlauts += "\xC3\xBC\xC3\xBC";
    const std::string parsed = ParseNameFile(umlauts);
    CHECK(parsed.size() == MAX_NAME_BYTES - 1);
    CHECK(parsed.back() == 'a');
}

TEST_CASE("Map names: only new map numbers look for a name file")
{
    CHECK(World::MapNames::Find(0) == nullptr);
    CHECK(World::MapNames::Find(World::MapNumbers::FIRST_NEW_MAP - 1) == nullptr);
    CHECK(World::MapNames::Find(World::MapNumbers::LAST_NEW_MAP + 1) == nullptr);
    CHECK(World::MapNumbers::FolderOf(World::MapNumbers::FIRST_NEW_MAP) == 83);
    CHECK(World::MapNumbers::LAST_NEW_MAP == 254);
}
