#include "GateQueries.h"

#ifdef _EDITOR

namespace Editor::Gates
{
namespace
{
const GateRecord* TargetOf(const GateTable& table, const GateRecord& enter)
{
    if (enter.flag != FLAG_ENTER || !IsValidNumber(enter.target))
        return nullptr;
    const GateRecord& target = table[enter.target];
    return (target.flag == FLAG_ARRIVAL || target.flag == FLAG_SPAWN) && !IsFree(target) ? &target : nullptr;
}
} // namespace

std::vector<int> GatesOnMap(const GateTable& table, int map)
{
    std::vector<int> numbers;
    for (int number = 0; number < GATE_COUNT; ++number)
    {
        const GateRecord& record = table[static_cast<std::size_t>(number)];
        if (!IsFree(record) && record.map == map)
            numbers.push_back(number);
    }
    return numbers;
}

std::vector<int> EnterGatesInto(const GateTable& table, int map)
{
    std::vector<int> numbers;
    for (int number = 0; number < GATE_COUNT; ++number)
    {
        const GateRecord& record = table[static_cast<std::size_t>(number)];
        const GateRecord* target = TargetOf(table, record);
        if (target != nullptr && record.map != map && target->map == map)
            numbers.push_back(number);
    }
    return numbers;
}

std::vector<int> EnterGatesTargeting(const GateTable& table, int arrival)
{
    std::vector<int> numbers;
    for (int number = 0; number < GATE_COUNT; ++number)
    {
        const GateRecord& record = table[static_cast<std::size_t>(number)];
        if (record.flag == FLAG_ENTER && record.target == arrival)
            numbers.push_back(number);
    }
    return numbers;
}

int TargetMap(const GateTable& table, int enter)
{
    if (!IsValidNumber(enter))
        return -1;
    const GateRecord* target = TargetOf(table, table[static_cast<std::size_t>(enter)]);
    return target != nullptr ? target->map : -1;
}
} // namespace Editor::Gates

#endif // _EDITOR
