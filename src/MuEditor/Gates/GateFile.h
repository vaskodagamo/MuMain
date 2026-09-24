#pragma once

#ifdef _EDITOR

#include "GateRecord.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Reading and writing Data/Gate.bmd (see GateRecord.h for the layout).
namespace Editor::Gates
{
// The cipher each record is stored with: BuxConvert (Core/Globals/_crypt.h), applied to
// one record at a time, so its key starts again at every record. It undoes itself.
using RecordCipher = std::function<void(std::uint8_t* record, std::size_t size)>;

// Fills `table` from the file's bytes. False with the reason, and `table` unchanged, when
// the file is not FILE_BYTES long (the client reads exactly 512 records).
bool DecodeGateFile(const std::vector<std::uint8_t>& file, const RecordCipher& cipher, GateTable& table,
                    std::string& error);

// The file's bytes for `table`: a table read with DecodeGateFile and not changed encodes
// to the bytes it was read from.
std::vector<std::uint8_t> EncodeGateFile(const GateTable& table, const RecordCipher& cipher);

// One record as the client holds it in memory (GATE_ATTRIBUTE, deciphered) and back.
GateRecord DecodeRecord(const std::uint8_t* plain);
void EncodeRecord(const GateRecord& record, std::uint8_t* plain);
} // namespace Editor::Gates

#endif // _EDITOR
