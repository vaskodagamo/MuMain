#include "GateRecord.h"

#ifdef _EDITOR

namespace Editor::Gates
{
bool IsFree(const GateRecord& record)
{
    return record.flag == 0 && record.map == 0 && record.x1 == 0 && record.y1 == 0 && record.x2 == 0 &&
           record.y2 == 0 && record.target == 0 && record.direction == 0 && record.padding == 0 && record.level == 0 &&
           record.maxLevel == 0;
}

bool IsCustomNumber(int number)
{
    return number >= FIRST_CUSTOM_GATE && number <= LAST_GATE;
}

bool IsValidNumber(int number)
{
    return number >= 0 && number <= LAST_GATE;
}

const char* KindName(const GateRecord& record)
{
    if (record.flag == FLAG_ENTER)
        return "enter";
    if (record.flag == FLAG_ARRIVAL)
        return "arrival";
    return IsFree(record) ? "free" : "spawn";
}
} // namespace Editor::Gates

#endif // _EDITOR
