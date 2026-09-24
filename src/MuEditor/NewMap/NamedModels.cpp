#include "NamedModels.h"

#ifdef _EDITOR

#include <cstdio>

namespace Editor::NewMap
{
namespace
{
constexpr const char* GENERIC_MODEL_PREFIX = "Object";
constexpr const char* MODEL_EXTENSION = ".bmd";
constexpr std::size_t FILE_NAME_CHARS = 64;

// "%s%02d.bmd" as CLoadData::AccessModel builds it: two digits up to 9, more above.
std::string NumberedFile(const char* baseName, int number)
{
    char name[FILE_NAME_CHARS];
    std::snprintf(name, sizeof(name), "%s%02d%s", baseName, number, MODEL_EXTENSION);
    return name;
}
} // namespace

const std::vector<NamedModelRun>& LorenciaModels()
{
    // CMapManager::LoadWorld, `WorldActive == 0`; the adapter checks each first type
    // against the engine's MODEL_* numbers.
    static const std::vector<NamedModelRun> runs = {
        {0, "Tree", 13},         {20, "Grass", 8},          {30, "Stone", 5},         {40, "StoneStatue", 3},
        {43, "SteelStatue", 1},  {44, "Tomb", 3},           {50, "FireLight", 2},     {52, "Bonfire", 1},
        {55, "DoungeonGate", 1}, {58, "TreasureDrum", 1},   {59, "TreasureChest", 1}, {60, "Ship", 1},
        {69, "StoneWall", 6},    {75, "StoneMuWall", 4},    {65, "SteelWall", 3},     {68, "SteelDoor", 1},
        {91, "Cannon", 3},       {80, "Bridge", 1},         {81, "Fence", 4},         {85, "BridgeStone", 1},
        {90, "StreetLight", 1},  {95, "Curtain", 1},        {98, "Carriage", 4},      {102, "Straw", 2},
        {96, "Sign", 2},         {56, "MerchantAnimal", 2}, {105, "Waterspout", 1},   {106, "Well", 4},
        {110, "Hanging", 1},     {115, "House", 5},         {120, "Tent", 1},         {111, "Stair", 1},
        {121, "HouseWall", 6},   {127, "HouseEtc", 3},      {130, "Light", 3},        {133, "PoseBox", 1},
        {140, "Furniture", 7},   {150, "Candle", 1},        {151, "Beer", 3},
    };
    return runs;
}

std::string GenericModelFile(int type)
{
    return NumberedFile(GENERIC_MODEL_PREFIX, type + 1);
}

std::string NamedModelFile(const NamedModelRun& run, int index)
{
    return NumberedFile(run.baseName, index + 1);
}
} // namespace Editor::NewMap

#endif // _EDITOR
