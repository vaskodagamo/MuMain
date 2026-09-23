#pragma once

#ifdef _EDITOR

#include "Core/Platform/WinCompat.h"

#include <cstddef>
#include <vector>

// The item table file as it was last loaded: its record layout and every record's
// decrypted bytes. The editor's save writes the same layout back and keeps the
// loaded bytes of each item nobody edited, so saving without an edit reproduces
// the file byte for byte. The conversion alone would not: the shipped files carry
// bytes it drops (text after a name's terminator, padding, names that are not
// valid UTF-8) and are in the legacy layout with 30-byte names.
// Editor builds only; the game keeps no copy.
class ItemFileSnapshot
{
public:
    // Called by the loader once the records are decrypted.
    static void Remember(const BYTE* records, size_t recordSize, size_t recordCount);

    // sizeof the file record the table was loaded from, 0 before a load.
    static size_t RecordSize();

    // The loaded bytes of record `index`, or nullptr when there is none.
    static const BYTE* Record(size_t index);

private:
    static std::vector<BYTE> s_records;
    static size_t s_recordSize;
};

#endif // _EDITOR
