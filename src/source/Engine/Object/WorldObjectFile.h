#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// The plain (decrypted) layout of Data/World{N}/EncTerrain{N}.obj, the list of
// world objects (trees, houses, fences...) a map places:
//
//   uint8 version, uint8 map number, int16 record count,
//   then per record: int16 model type, float position[3], float angle[3], float scale
//
// 4 header bytes and 30 bytes per record, little-endian like every platform the
// client runs on. On disk the whole file is MapFileEncrypt'ed. The records are a
// flat list; the loader sorts them into the 16x16 object blocks by position.
namespace Engine::Object::WorldObjectFile
{
constexpr std::size_t HEADER_BYTES = 4;
constexpr std::size_t RECORD_BYTES = 30;
// The record count is a signed 16-bit field.
constexpr std::size_t MAX_RECORDS = 32767;

struct Record
{
    std::int16_t type = 0;
    float position[3] = {};
    float angle[3] = {};
    float scale = 1.0f;
};

// A record with the place it takes in a saved file (see InSaveOrder).
constexpr int NO_SAVE_ORDER = -1;
struct OrderedRecord
{
    int order = NO_SAVE_ORDER;
    Record record;
};

struct Contents
{
    std::uint8_t version = 0;
    std::uint8_t mapNumber = 0;
    std::vector<Record> records;
};

// Builds the plain bytes of `contents`. Returns an empty vector when it has more
// than MAX_RECORDS records, which the count field cannot hold.
std::vector<std::uint8_t> Encode(const Contents& contents);

// The records sorted by `order`, then those without one (NO_SAVE_ORDER or any
// other negative value) in the order given. The Map Editor gives each object the
// index of the record it was loaded from, so saving a map without edits writes its
// file unchanged, and numbers the objects it adds after them.
std::vector<Record> InSaveOrder(std::vector<OrderedRecord> records);

// The same order for any list: the positions of `orders` (one per item, each an
// item's save order or NO_SAVE_ORDER) sorted the way InSaveOrder sorts records.
std::vector<std::size_t> SaveOrderIndices(const std::vector<int>& orders);

// Parses plain bytes into `out`. Returns false when the header is missing, the
// count is negative, or the data ends before the last counted record; `out` then
// holds the header (when present) and the complete records that were there.
bool Decode(const std::uint8_t* data, std::size_t size, Contents& out);
} // namespace Engine::Object::WorldObjectFile
