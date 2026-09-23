#include "GateFile.h"

#ifdef _EDITOR

#include <algorithm>

namespace Editor::Gates
{
namespace
{
// Offsets inside one record (GateRecord.h).
constexpr std::size_t FLAG_OFFSET = 0;
constexpr std::size_t MAP_OFFSET = 1;
constexpr std::size_t X1_OFFSET = 2;
constexpr std::size_t Y1_OFFSET = 3;
constexpr std::size_t X2_OFFSET = 4;
constexpr std::size_t Y2_OFFSET = 5;
constexpr std::size_t TARGET_OFFSET = 6;
constexpr std::size_t DIRECTION_OFFSET = 8;
constexpr std::size_t PADDING_OFFSET = 9;
constexpr std::size_t LEVEL_OFFSET = 10;
constexpr std::size_t MAX_LEVEL_OFFSET = 12;
constexpr int BITS_PER_BYTE = 8;
constexpr unsigned LOW_BYTE_MASK = 0xFF;

std::uint16_t ReadWord(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << BITS_PER_BYTE));
}

void WriteWord(std::uint16_t value, std::uint8_t* bytes)
{
    bytes[0] = static_cast<std::uint8_t>(value & LOW_BYTE_MASK);
    bytes[1] = static_cast<std::uint8_t>(value >> BITS_PER_BYTE);
}
} // namespace

GateRecord DecodeRecord(const std::uint8_t* plain)
{
    GateRecord record;
    record.flag = plain[FLAG_OFFSET];
    record.map = plain[MAP_OFFSET];
    record.x1 = plain[X1_OFFSET];
    record.y1 = plain[Y1_OFFSET];
    record.x2 = plain[X2_OFFSET];
    record.y2 = plain[Y2_OFFSET];
    record.target = ReadWord(plain + TARGET_OFFSET);
    record.direction = plain[DIRECTION_OFFSET];
    record.padding = plain[PADDING_OFFSET];
    record.level = ReadWord(plain + LEVEL_OFFSET);
    record.maxLevel = ReadWord(plain + MAX_LEVEL_OFFSET);
    return record;
}

void EncodeRecord(const GateRecord& record, std::uint8_t* plain)
{
    plain[FLAG_OFFSET] = record.flag;
    plain[MAP_OFFSET] = record.map;
    plain[X1_OFFSET] = record.x1;
    plain[Y1_OFFSET] = record.y1;
    plain[X2_OFFSET] = record.x2;
    plain[Y2_OFFSET] = record.y2;
    WriteWord(record.target, plain + TARGET_OFFSET);
    plain[DIRECTION_OFFSET] = record.direction;
    plain[PADDING_OFFSET] = record.padding;
    WriteWord(record.level, plain + LEVEL_OFFSET);
    WriteWord(record.maxLevel, plain + MAX_LEVEL_OFFSET);
}

bool DecodeGateFile(const std::vector<std::uint8_t>& file, const RecordCipher& cipher, GateTable& table,
                    std::string& error)
{
    if (file.size() != FILE_BYTES)
    {
        error = "Gate.bmd is " + std::to_string(file.size()) + " bytes; the client reads exactly " +
                std::to_string(FILE_BYTES) + " (512 records of 14)";
        return false;
    }
    GateTable decoded;
    std::uint8_t plain[RECORD_BYTES];
    for (int number = 0; number < GATE_COUNT; ++number)
    {
        std::copy_n(file.data() + number * RECORD_BYTES, RECORD_BYTES, plain);
        cipher(plain, RECORD_BYTES);
        decoded[number] = DecodeRecord(plain);
    }
    table = decoded;
    return true;
}

std::vector<std::uint8_t> EncodeGateFile(const GateTable& table, const RecordCipher& cipher)
{
    std::vector<std::uint8_t> file(FILE_BYTES);
    for (int number = 0; number < GATE_COUNT; ++number)
    {
        std::uint8_t* record = file.data() + number * RECORD_BYTES;
        EncodeRecord(table[number], record);
        cipher(record, RECORD_BYTES);
    }
    return file;
}
} // namespace Editor::Gates

#endif // _EDITOR
