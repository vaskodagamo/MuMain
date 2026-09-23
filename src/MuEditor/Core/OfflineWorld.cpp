#include "stdafx.h"

#ifdef _EDITOR

#include "OfflineWorld.h"

#include "EditorCamera.h"
#include "LiveMap.h"
#include "MuEditorCore.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "UI/MapEditor/MapEditorFileUtil.h"
#include "UI/MapEditor/MapEditorUI.h"

#include "Camera/FreeFlyCamera.h"
#include "Core/Utilities/StringUtils.h"
#include "Engine/Object/ZzzCharacter.h"   // Hero
#include "Engine/Object/ZzzInterface.h"   // LoadingWorld
#include "Engine/Object/ZzzObject.h"      // ObjectBlock
#include "Network/Server/WSclient.h"      // CurrentProtocolState
#include "Render/Terrain/ZzzLodTerrain.h" // RequestTerrainHeight
#include "Scenes/SceneCommon.h"           // InitMainScene, EnableMainRender
#include "Scenes/SceneCore.h"             // SceneFlag
#include "UI/Legacy/UIMng.h"
#include "World/MapInfra/MapManager.h"
#include "World/MapInfra/MapNumbers.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <string>
#include <vector>

namespace Editor::OfflineWorld
{
namespace
{
constexpr const wchar_t* WORLD_OPTION = L"--world";
constexpr wchar_t WORLD_OPTION_SEPARATOR = L'='; // "--world=3" works as well as "--world 3"
constexpr int DECIMAL_BASE = 10;
constexpr int NO_WORLD = 0;

// Data folders count from 1 (World1 = Lorencia), the engine's map index
// (gMapManager.WorldActive) from 0. Any folder that exists opens, up to the last one a
// map number can have, so new maps (82 and up, World83 and up) open like the game's own.
constexpr int FIRST_WORLD_FOLDER = World::MapNumbers::FIRST_FOLDER;
constexpr int LAST_WORLD_FOLDER = World::MapNumbers::LAST_FOLDER;

// Start view. The hidden hero and the camera target sit at the start point
// (see FindStartPoint). The camera looks along the game camera's heading,
// 45 degrees down (FreeFly pitch: 0 = straight down, -90 = level), from far
// enough away to show several blocks of houses.
constexpr float MAP_CENTRE = TERRAIN_SIZE * TERRAIN_SCALE * 0.5f;
constexpr float START_YAW = -45.0f;
constexpr float START_PITCH = -45.0f;
constexpr float START_DISTANCE = 6000.0f;

// A point on the map in world units; its height comes from the terrain.
struct GroundPoint
{
    float x;
    float y;
};

int s_requestedWorld = NO_WORLD;
bool s_active = false;
GroundPoint s_startPoint{MAP_CENTRE, MAP_CENTRE};

void Log(const std::wstring& message)
{
    g_ErrorReport.Write(L"%ls\r\n", message.c_str());
    g_MuEditorConsoleUI.LogEditor(StringUtils::WideToNarrow(message.c_str()));
}

// Opens the file the way the engine's loaders do (_wfopen resolves the path
// and its letter case on every platform).
bool CanOpen(const std::filesystem::path& file)
{
    FILE* fp = _wfopen(file.wstring().c_str(), L"rb");
    if (fp == nullptr)
        return false;
    fclose(fp);
    return true;
}

// The first file LoadWorld needs from Data/World{world} that cannot be opened,
// or an empty path when all of them are there. Each missing one would end the
// client with a message box, so they are checked before loading.
std::filesystem::path FindMissingWorldFile(int world)
{
    const std::filesystem::path requiredFiles[] = {
        Editor::Files::TerrainMappingFile(world), Editor::Files::TerrainAttributeFile(world),
        Editor::Files::TerrainObjectFile(world),  Editor::Files::TerrainHeightFile(world),
        Editor::Files::TerrainLightFile(world),
    };
    for (const std::filesystem::path& file : requiredFiles)
    {
        if (!CanOpen(file))
            return file;
    }
    return {};
}

// Reorders `values` (not empty) and returns their median.
float Median(std::vector<float>& values)
{
    const auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}

// The middle of the loaded map's objects, the median on each axis so a few
// far-off trees do not pull it away: Lorencia's town, and the castle of Blood
// Castle, whose content lies far from the map centre. The map centre when the
// map has no objects.
GroundPoint FindStartPoint()
{
    std::vector<float> xs;
    std::vector<float> ys;
    for (const OBJECT_BLOCK& block : ObjectBlock)
    {
        for (const OBJECT* object = block.Head; object != nullptr; object = object->Next)
        {
            xs.push_back(object->Position[0]);
            ys.push_back(object->Position[1]);
        }
    }
    if (xs.empty())
        return {MAP_CENTRE, MAP_CENTRE};
    return {Median(xs), Median(ys)};
}

void GroundPosition(const GroundPoint& point, vec3_t position)
{
    Vector(point.x, point.y, RequestTerrainHeight(point.x, point.y), position);
}

void LoadMap(int world)
{
    // The in-game map join also loads while the main scene is up.
    SceneFlag = MAIN_SCENE;
    gMapManager.WorldActive = world - FIRST_WORLD_FOLDER;
    gMapManager.LoadWorld(gMapManager.WorldActive);
}

// What the server's map-join reply and InitializeMainScene would set, minus the
// character: the main scene renders at once, with no loading countdown.
void StartMainSceneWithoutServer()
{
    InitMainScene = true; // skips InitializeMainScene, which asks the server for the character
    EnableMainRender = true;
    CurrentProtocolState = RECEIVE_JOIN_MAP_SERVER;
    LoadingWorld = 0;
    CUIMng::Instance().CreateMainScene();
}

// Hero always points at a character slot, and falling leaves, birds and
// ambient sounds follow its position. Park an inactive, invisible one at the
// start point; a hero that is not Live never moves and is never drawn.
void PlaceHiddenHero()
{
    OBJECT& hero = Hero->Object;
    hero.Live = false;
    GroundPosition(s_startPoint, hero.Position);
    VectorCopy(hero.Position, hero.StartPosition);
}

// After a map loaded: the hidden hero and the camera go to its start point, and the
// map as loaded becomes what "unsaved" edits are measured against.
void ShowStartView()
{
    s_startPoint = FindStartPoint();
    PlaceHiddenHero();
    ResetCamera();
    Editor::LiveMap::SyncWithLoadedMap();
}
} // namespace

void ReadCommandLine(const wchar_t* commandLine)
{
    const wchar_t* option = commandLine != nullptr ? wcsstr(commandLine, WORLD_OPTION) : nullptr;
    if (option == nullptr)
        return;

    const wchar_t* value = option + wcslen(WORLD_OPTION);
    if (*value == WORLD_OPTION_SEPARATOR)
        ++value;

    wchar_t* valueEnd = nullptr;
    const long world = wcstol(value, &valueEnd, DECIMAL_BASE); // skips the blank after --world
    if (valueEnd == value || world < FIRST_WORLD_FOLDER || world > LAST_WORLD_FOLDER)
    {
        Log(L"[Editor] --world needs a map folder number from " + std::to_wstring(FIRST_WORLD_FOLDER) + L" to " +
            std::to_wstring(LAST_WORLD_FOLDER) + L" (1 = Lorencia) - starting the normal login instead.");
        return;
    }
    s_requestedWorld = static_cast<int>(world);
}

bool TryEnter()
{
    const int world = s_requestedWorld;
    s_requestedWorld = NO_WORLD;
    if (world == NO_WORLD)
        return false;

    const std::filesystem::path missing = FindMissingWorldFile(world);
    if (!missing.empty())
    {
        Log(L"[Editor] --world " + std::to_wstring(world) + L": " + missing.wstring() +
            L" is missing - starting the normal login instead.");
        return false;
    }

    // Active before loading, so map set-up code already knows there is no server.
    s_active = true;
    LoadMap(world);
    StartMainSceneWithoutServer();
    ShowStartView();
    g_MuEditorCore.ShowMapEditor();

    Log(L"[Editor] Opened World" + std::to_wstring(world) + L" offline: no server, no login, free-fly camera.");
    return true;
}

bool Open(int world, std::string& error)
{
    if (!s_active)
    {
        error = "no map is open offline; start the client with --editor --world N";
        return false;
    }
    if (world < FIRST_WORLD_FOLDER || world > LAST_WORLD_FOLDER)
    {
        error = "the map folder number runs from " + std::to_string(FIRST_WORLD_FOLDER) + " to " +
                std::to_string(LAST_WORLD_FOLDER);
        return false;
    }
    const std::filesystem::path missing = FindMissingWorldFile(world);
    if (!missing.empty())
    {
        error = StringUtils::WideToNarrow(missing.wstring().c_str()) + " is missing";
        return false;
    }

    LoadMap(world);
    // Loading freed every object: the selection and the undo steps went with them.
    g_MapEditorUI.ForgetUnloadedMap();
    ShowStartView();
    Log(L"[Editor] Switched to World" + std::to_wstring(world) + L" offline.");
    return true;
}

bool IsActive()
{
    return s_active;
}

void ResetCamera()
{
    // The first offline frame is then culled by FreeFly and not by the game
    // camera, which still sits at the world origin.
    FreeFlyCamera* camera = Editor::Camera::ActivateFreeFly();
    if (camera == nullptr)
    {
        Log(L"[Editor] Could not switch to the free-fly camera.");
        return;
    }

    vec3_t target;
    GroundPosition(s_startPoint, target);
    camera->LookAt(target, START_YAW, START_PITCH, START_DISTANCE);
}
} // namespace Editor::OfflineWorld

#endif // _EDITOR
