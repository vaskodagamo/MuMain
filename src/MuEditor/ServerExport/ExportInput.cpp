#include "ExportInput.h"

#ifdef _EDITOR

#include "Gates/GateQueries.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>

namespace Editor::ServerExport
{
namespace
{
namespace Gates = Editor::Gates;

void AddUnique(std::vector<int>& numbers, int number)
{
    if (std::find(numbers.begin(), numbers.end(), number) == numbers.end())
        numbers.push_back(number);
}

std::vector<int> OnlyCustom(const std::vector<int>& numbers)
{
    std::vector<int> custom;
    std::copy_if(numbers.begin(), numbers.end(), std::back_inserter(custom), Gates::IsCustomNumber);
    return custom;
}

// The arrival enter gate `enter` lands on, filed by the map it lies on.
void AddTarget(const Gates::GateTable& table, int map, int enter, ExportGates& gates)
{
    const int targetMap = Gates::TargetMap(table, enter);
    if (targetMap < 0)
        return;
    const int target = table[static_cast<std::size_t>(enter)].target;
    AddUnique(targetMap == map ? gates.arrivalsOnMap : gates.arrivalsElsewhere, target);
}
} // namespace

ExportGates CollectGates(const Gates::GateTable& table, int map)
{
    ExportGates gates;
    for (const int number : OnlyCustom(Gates::GatesOnMap(table, map)))
    {
        const Gates::GateRecord& record = table[static_cast<std::size_t>(number)];
        if (record.flag == Gates::FLAG_ENTER)
            gates.enterOnMap.push_back(number);
        else if (record.flag == Gates::FLAG_ARRIVAL)
            AddUnique(gates.arrivalsOnMap, number);
    }
    gates.enterInto = OnlyCustom(Gates::EnterGatesInto(table, map));
    for (const std::vector<int>* enters : {&gates.enterOnMap, &gates.enterInto})
    {
        for (const int enter : *enters)
            AddTarget(table, map, enter, gates);
    }
    std::sort(gates.arrivalsOnMap.begin(), gates.arrivalsOnMap.end());
    return gates;
}

std::vector<int> MapsTouched(const ExportInput& input)
{
    std::vector<int> maps = {input.map};
    if (input.gates == nullptr)
        return maps;
    const ExportGates& set = input.gateSet;
    for (const auto* numbers : {&set.enterInto, &set.arrivalsElsewhere})
    {
        for (const int number : *numbers)
            AddUnique(maps, (*input.gates)[static_cast<std::size_t>(number)].map);
    }
    return maps;
}
} // namespace Editor::ServerExport

#endif // _EDITOR
