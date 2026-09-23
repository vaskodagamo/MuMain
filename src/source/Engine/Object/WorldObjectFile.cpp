#include "WorldObjectFile.h"

#include <algorithm>
#include <cstring>

namespace Engine::Object::WorldObjectFile
{
namespace
{
constexpr std::size_t VERSION_OFFSET = 0;
constexpr std::size_t MAP_NUMBER_OFFSET = 1;
constexpr std::size_t COUNT_OFFSET = 2;

template <typename T> void Append(std::vector<std::uint8_t>& out, const T& value)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

template <typename T> void Read(const std::uint8_t*& cursor, T& value)
{
    std::memcpy(&value, cursor, sizeof(T));
    cursor += sizeof(T);
}

void AppendRecord(std::vector<std::uint8_t>& out, const Record& record)
{
    Append(out, record.type);
    Append(out, record.position);
    Append(out, record.angle);
    Append(out, record.scale);
}

Record ReadRecord(const std::uint8_t* cursor)
{
    Record record;
    Read(cursor, record.type);
    Read(cursor, record.position);
    Read(cursor, record.angle);
    Read(cursor, record.scale);
    return record;
}

static_assert(sizeof(Record::type) + sizeof(Record::position) + sizeof(Record::angle) + sizeof(Record::scale) ==
                  RECORD_BYTES,
              "an .obj record is 30 bytes");
} // namespace

std::vector<std::uint8_t> Encode(const Contents& contents)
{
    if (contents.records.size() > MAX_RECORDS)
        return {};

    std::vector<std::uint8_t> out;
    out.reserve(HEADER_BYTES + contents.records.size() * RECORD_BYTES);
    Append(out, contents.version);
    Append(out, contents.mapNumber);
    Append(out, static_cast<std::int16_t>(contents.records.size()));
    for (const Record& record : contents.records)
        AppendRecord(out, record);
    return out;
}

std::vector<Record> InSaveOrder(std::vector<OrderedRecord> records)
{
    const auto comesFirst = [](const OrderedRecord& a, const OrderedRecord& b)
    {
        const bool aOrdered = a.order >= 0;
        const bool bOrdered = b.order >= 0;
        if (aOrdered != bOrdered)
            return aOrdered;
        return aOrdered && a.order < b.order;
    };
    std::stable_sort(records.begin(), records.end(), comesFirst);

    std::vector<Record> sorted;
    sorted.reserve(records.size());
    for (const OrderedRecord& ordered : records)
        sorted.push_back(ordered.record);
    return sorted;
}

bool Decode(const std::uint8_t* data, std::size_t size, Contents& out)
{
    out = Contents{};
    if (data == nullptr || size < HEADER_BYTES)
        return false;

    out.version = data[VERSION_OFFSET];
    out.mapNumber = data[MAP_NUMBER_OFFSET];
    std::int16_t count = 0;
    std::memcpy(&count, data + COUNT_OFFSET, sizeof(count));
    if (count < 0)
        return false;

    const std::size_t available = (size - HEADER_BYTES) / RECORD_BYTES;
    const std::size_t complete = std::min(available, static_cast<std::size_t>(count));
    out.records.reserve(complete);
    for (std::size_t i = 0; i < complete; ++i)
        out.records.push_back(ReadRecord(data + HEADER_BYTES + i * RECORD_BYTES));
    return complete == static_cast<std::size_t>(count);
}
} // namespace Engine::Object::WorldObjectFile
