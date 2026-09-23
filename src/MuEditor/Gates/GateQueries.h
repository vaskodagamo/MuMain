#pragma once

#ifdef _EDITOR

#include "GateRecord.h"

#include <vector>

// Finding gates in the table: those on a map, those leading to it, and who lands where.
namespace Editor::Gates
{
// The gates whose area lies on `map` (enter, arrival and spawn records), lowest first.
std::vector<int> GatesOnMap(const GateTable& table, int map);

// Enter gates on other maps whose arrival gate lies on `map`: the ways in.
std::vector<int> EnterGatesInto(const GateTable& table, int map);

// The enter gates whose target is gate `arrival`.
std::vector<int> EnterGatesTargeting(const GateTable& table, int arrival);

// The map enter gate `enter` leads to; -1 when its target is not a gate number or not an
// arrival or spawn record.
int TargetMap(const GateTable& table, int enter);
} // namespace Editor::Gates

#endif // _EDITOR
