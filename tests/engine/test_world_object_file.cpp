#include "App/stdafx.h"

#include <doctest.h>

#include "Engine/Object/WorldObjectFile.h"
#include "Render/Terrain/ZzzLodTerrain.h" // MapFileEncrypt / MapFileDecrypt

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace ObjectFile = Engine::Object::WorldObjectFile;

namespace
{
using Bytes = std::vector<std::uint8_t>;

constexpr std::uint8_t LORENCIA_MAP_NUMBER = 1;
constexpr std::int16_t FIRST_WORLD_TYPE = 0;
constexpr std::int16_t LORENCIA_HIGHEST_TYPE = 153;
constexpr std::int16_t LOGIN_SCENE_TYPE = 247; // above MAX_WORLD_OBJECTS, used by World58's file

ObjectFile::Record MakeRecord(std::int16_t type, float seed)
{
    ObjectFile::Record record;
    record.type = type;
    for (int i = 0; i < 3; ++i)
    {
        record.position[i] = seed * 1000.0f + static_cast<float>(i);
        record.angle[i] = seed * 10.0f - static_cast<float>(i);
    }
    record.scale = seed;
    return record;
}

bool SameRecord(const ObjectFile::Record& a, const ObjectFile::Record& b)
{
    return a.type == b.type && std::memcmp(a.position, b.position, sizeof(a.position)) == 0 &&
           std::memcmp(a.angle, b.angle, sizeof(a.angle)) == 0 && a.scale == b.scale;
}

Bytes Encrypt(Bytes plain)
{
    Bytes encrypted(plain.size());
    MapFileEncrypt(encrypted.data(), plain.data(), static_cast<int>(plain.size()));
    return encrypted;
}

Bytes Decrypt(Bytes encrypted)
{
    Bytes plain(encrypted.size());
    MapFileDecrypt(plain.data(), encrypted.data(), static_cast<int>(encrypted.size()));
    return plain;
}

Bytes ReadFile(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::vector<fs::path> ShippedObjectFiles()
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const fs::directory_entry& world : fs::directory_iterator(fs::path(MU_TEST_DATA_DIR), ec))
    {
        if (!world.is_directory() || world.path().filename().string().rfind("World", 0) != 0)
            continue;
        for (const fs::directory_entry& file : fs::directory_iterator(world.path(), ec))
        {
            const std::string name = file.path().filename().string();
            if (name.rfind("EncTerrain", 0) == 0 && file.path().extension() == ".obj")
                files.push_back(file.path());
        }
    }
    return files;
}

// The loader sorts records into a 16x16 grid of blocks, each 16x16 tiles of 100
// world units (CreateObject); a record outside it is not loaded.
constexpr float BLOCK_SPAN = 16.0f * 100.0f;
constexpr int GRID_BLOCKS = 16;

int BlockOf(const ObjectFile::Record& record)
{
    const int i = static_cast<int>(record.position[0] / BLOCK_SPAN);
    const int j = static_cast<int>(record.position[1] / BLOCK_SPAN);
    if (i < 0 || j < 0 || i >= GRID_BLOCKS || j >= GRID_BLOCKS)
        return -1;
    return i * GRID_BLOCKS + j;
}

// What SaveObjects collects after the Map Editor build loaded `records`: the
// objects block by block, each block in load order, each with the index of the
// record it came from (OBJECT::SaveOrder).
std::vector<ObjectFile::OrderedRecord> LoadIntoBlocks(const std::vector<ObjectFile::Record>& records)
{
    std::vector<std::vector<ObjectFile::OrderedRecord>> blocks(GRID_BLOCKS * GRID_BLOCKS);
    for (size_t i = 0; i < records.size(); ++i)
    {
        const int block = BlockOf(records[i]);
        if (block >= 0)
            blocks[block].push_back({static_cast<int>(i), records[i]});
    }
    std::vector<ObjectFile::OrderedRecord> collected;
    for (const auto& block : blocks)
        collected.insert(collected.end(), block.begin(), block.end());
    return collected;
}

Bytes SaveAs(const ObjectFile::Contents& loaded, std::vector<ObjectFile::Record> records)
{
    ObjectFile::Contents saved;
    saved.version = loaded.version;
    saved.mapNumber = loaded.mapNumber;
    saved.records = std::move(records);
    return Encrypt(ObjectFile::Encode(saved));
}
} // namespace

TEST_CASE("An encrypted object file decodes to the records that were saved [engine][objects]")
{
    ObjectFile::Contents saved;
    saved.mapNumber = LORENCIA_MAP_NUMBER;
    saved.records = {MakeRecord(FIRST_WORLD_TYPE, 1.5f), MakeRecord(LORENCIA_HIGHEST_TYPE, -2.25f),
                     MakeRecord(LOGIN_SCENE_TYPE, 0.75f)};

    const Bytes plain = ObjectFile::Encode(saved);
    CHECK(plain.size() == ObjectFile::HEADER_BYTES + saved.records.size() * ObjectFile::RECORD_BYTES);

    const Bytes onDisk = Encrypt(plain);
    CHECK(onDisk != plain);

    const Bytes decrypted = Decrypt(onDisk);
    ObjectFile::Contents loaded;
    REQUIRE(ObjectFile::Decode(decrypted.data(), decrypted.size(), loaded));
    CHECK(loaded.version == saved.version);
    CHECK(loaded.mapNumber == saved.mapNumber);
    REQUIRE(loaded.records.size() == saved.records.size());
    for (size_t i = 0; i < saved.records.size(); ++i)
        CHECK(SameRecord(loaded.records[i], saved.records[i]));
}

TEST_CASE("The header counts exactly the records written [engine][objects]")
{
    ObjectFile::Contents saved;
    saved.version = 0;
    saved.mapNumber = LORENCIA_MAP_NUMBER;
    saved.records = {MakeRecord(FIRST_WORLD_TYPE, 1.0f), MakeRecord(LORENCIA_HIGHEST_TYPE, 2.0f)};

    const Bytes plain = ObjectFile::Encode(saved);
    REQUIRE(plain.size() >= ObjectFile::HEADER_BYTES);
    std::int16_t count = 0;
    std::memcpy(&count, plain.data() + 2, sizeof(count));
    CHECK(plain[0] == saved.version);
    CHECK(plain[1] == saved.mapNumber);
    CHECK(count == 2);
    std::int16_t firstType = -1;
    std::memcpy(&firstType, plain.data() + ObjectFile::HEADER_BYTES, sizeof(firstType));
    CHECK(firstType == FIRST_WORLD_TYPE);
}

TEST_CASE("A broken object file yields only its complete records [engine][objects]")
{
    ObjectFile::Contents saved;
    saved.mapNumber = LORENCIA_MAP_NUMBER;
    saved.records = {MakeRecord(FIRST_WORLD_TYPE, 1.0f), MakeRecord(1, 2.0f), MakeRecord(2, 3.0f)};
    Bytes plain = ObjectFile::Encode(saved);

    ObjectFile::Contents loaded;
    CHECK_FALSE(ObjectFile::Decode(plain.data(), ObjectFile::HEADER_BYTES - 1, loaded));
    CHECK(loaded.records.empty());

    const size_t cutAfterTwoRecords = ObjectFile::HEADER_BYTES + 2 * ObjectFile::RECORD_BYTES + 7;
    CHECK_FALSE(ObjectFile::Decode(plain.data(), cutAfterTwoRecords, loaded));
    CHECK(loaded.mapNumber == LORENCIA_MAP_NUMBER);
    REQUIRE(loaded.records.size() == 2);
    CHECK(SameRecord(loaded.records[1], saved.records[1]));

    const std::int16_t negativeCount = -1;
    std::memcpy(plain.data() + 2, &negativeCount, sizeof(negativeCount));
    CHECK_FALSE(ObjectFile::Decode(plain.data(), plain.size(), loaded));
    CHECK(loaded.records.empty());
}

TEST_CASE("More objects than the count field holds are refused [engine][objects]")
{
    ObjectFile::Contents contents;
    contents.records.resize(ObjectFile::MAX_RECORDS);
    CHECK(ObjectFile::Encode(contents).size() ==
          ObjectFile::HEADER_BYTES + ObjectFile::MAX_RECORDS * ObjectFile::RECORD_BYTES);

    contents.records.resize(ObjectFile::MAX_RECORDS + 1);
    CHECK(ObjectFile::Encode(contents).empty());
}

TEST_CASE("Every shipped object file decodes and re-encodes byte for byte [engine][objects]")
{
    const std::vector<fs::path> files = ShippedObjectFiles();
    REQUIRE_FALSE(files.empty());

    for (const fs::path& file : files)
    {
        CAPTURE(file.string());
        const Bytes onDisk = ReadFile(file);
        const Bytes plain = Decrypt(onDisk);

        ObjectFile::Contents contents;
        REQUIRE(ObjectFile::Decode(plain.data(), plain.size(), contents));
        CHECK(Encrypt(ObjectFile::Encode(contents)) == onDisk);
    }
}

TEST_CASE("Saving Lorencia's objects without edits writes EncTerrain1.obj unchanged [engine][objects]")
{
    const Bytes onDisk = ReadFile(fs::path(MU_TEST_DATA_DIR) / "World1" / "EncTerrain1.obj");
    REQUIRE_FALSE(onDisk.empty());
    const Bytes plain = Decrypt(onDisk);
    ObjectFile::Contents loaded;
    REQUIRE(ObjectFile::Decode(plain.data(), plain.size(), loaded));

    const std::vector<ObjectFile::OrderedRecord> collected = LoadIntoBlocks(loaded.records);
    REQUIRE(collected.size() == loaded.records.size()); // every record lies on the map
    CHECK(SaveAs(loaded, ObjectFile::InSaveOrder(collected)) == onDisk);

    // Without the record indices the save walks the blocks, and Lorencia's file lists
    // its records in another order: that save renumbers them.
    std::vector<ObjectFile::OrderedRecord> unordered = collected;
    for (ObjectFile::OrderedRecord& object : unordered)
        object.order = ObjectFile::NO_SAVE_ORDER;
    CHECK(SaveAs(loaded, ObjectFile::InSaveOrder(unordered)) != onDisk);
}

TEST_CASE("Every shipped object file saves back unchanged in its record order [engine][objects]")
{
    for (const fs::path& file : ShippedObjectFiles())
    {
        CAPTURE(file.string());
        const Bytes plain = Decrypt(ReadFile(file));
        ObjectFile::Contents loaded;
        REQUIRE(ObjectFile::Decode(plain.data(), plain.size(), loaded));

        // Records the loader cannot place drop out; all others keep their order.
        std::vector<ObjectFile::Record> placeable;
        for (const ObjectFile::Record& record : loaded.records)
            if (BlockOf(record) >= 0 && record.type >= 0)
                placeable.push_back(record);
        std::vector<ObjectFile::OrderedRecord> collected;
        for (const ObjectFile::OrderedRecord& object : LoadIntoBlocks(loaded.records))
            if (object.record.type >= 0)
                collected.push_back(object);
        CHECK(SaveAs(loaded, ObjectFile::InSaveOrder(collected)) == SaveAs(loaded, placeable));
    }
}

TEST_CASE("Edited objects keep their record numbers, added ones follow and deleted ones drop out [engine][objects]")
{
    std::vector<ObjectFile::Record> file;
    for (int i = 0; i < 6; ++i)
        file.push_back(MakeRecord(static_cast<std::int16_t>(i), 0.5f + static_cast<float>(i)));
    std::vector<ObjectFile::OrderedRecord> collected = LoadIntoBlocks(file);
    REQUIRE(collected.size() == file.size());

    // Delete record 1, move record 4 (a re-created object keeps its order), add two
    // objects in the editor (numbered after the file) and one the game spawned.
    collected.erase(std::find_if(collected.begin(), collected.end(),
                                 [](const ObjectFile::OrderedRecord& object) { return object.order == 1; }));
    for (ObjectFile::OrderedRecord& object : collected)
        if (object.order == 4)
            object.record.position[0] = 12345.0f;
    const ObjectFile::OrderedRecord spawned{ObjectFile::NO_SAVE_ORDER, MakeRecord(99, 9.0f)};
    const ObjectFile::OrderedRecord firstAdded{6, MakeRecord(40, 7.0f)};
    const ObjectFile::OrderedRecord secondAdded{7, MakeRecord(41, 8.0f)};
    collected.insert(collected.begin(), spawned);
    collected.insert(collected.begin() + 1, secondAdded);
    collected.push_back(firstAdded);

    const std::vector<ObjectFile::Record> saved = ObjectFile::InSaveOrder(collected);
    REQUIRE(saved.size() == 8);
    const std::vector<std::int16_t> types = {0, 2, 3, 4, 5, 40, 41, 99};
    for (size_t i = 0; i < types.size(); ++i)
        CHECK(saved[i].type == types[i]);
    CHECK(saved[3].position[0] == 12345.0f);
}
