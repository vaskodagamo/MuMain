#include "stdafx.h"

#ifdef _EDITOR

#include "ItemFileSnapshot.h"

std::vector<BYTE> ItemFileSnapshot::s_records;
size_t ItemFileSnapshot::s_recordSize = 0;

void ItemFileSnapshot::Remember(const BYTE* records, size_t recordSize, size_t recordCount)
{
    s_records.assign(records, records + recordSize * recordCount);
    s_recordSize = recordSize;
}

size_t ItemFileSnapshot::RecordSize()
{
    return s_recordSize;
}

const BYTE* ItemFileSnapshot::Record(size_t index)
{
    const size_t offset = index * s_recordSize;
    if (s_recordSize == 0 || offset + s_recordSize > s_records.size())
        return nullptr;
    return s_records.data() + offset;
}

#endif // _EDITOR
