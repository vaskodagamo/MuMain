#include "stdafx.h"

#ifdef _EDITOR

#include "LiveMap.h"

#include "Assets/AssetCatalog.h"
#include "Assets/EditorText.h"
#include "MapInspect/AttributeBits.h"
#include "MapInspect/MapObjectList.h"
#include "MapInspect/SavedMapState.h"
#include "MapInspect/TilePalette.h"
#include "UI/MapEditor/MapEditorFileUtil.h"
#include "UI/MapEditor/MapObjectPlace.h"

#include "Core/Globals/_TextureIndex.h" // BITMAP_MAPTILE
#include "Core/Text/Utf8.h"
#include "Engine/Object/w_ObjectInfo.h"   // class OBJECT
#include "Engine/Object/ZzzInfomation.h"  // GateAttribute
#include "Engine/Object/ZzzObject.h"      // IsSavedWorldObject, ObjectListGeneration
#include "Render/Sprites/GlobalBitmap.h"  // Bitmaps
#include "Render/Terrain/ZzzLodTerrain.h" // terrain arrays, RequestTerrainHeight
#include "World/MapInfra/MapManager.h"

#include <optional>
#include <system_error>

namespace Editor::LiveMap
{
namespace
{
namespace Inspect = Editor::MapInspect;

static_assert(TERRAIN_SIZE == Inspect::MAP_TILES, "the inspection units assume 256 x 256 maps");
static_assert(sizeof(TerrainWall[0]) == sizeof(std::uint16_t), "TerrainWall holds 16-bit attributes");
static_assert(sizeof(TerrainLight[0]) == 3 * sizeof(float), "TerrainLight holds three floats per cell");
static_assert(TW_SAFEZONE == Inspect::Attribute::SAFEZONE && TW_CHARACTER == Inspect::Attribute::CHARACTER &&
                  TW_NOMOVE == Inspect::Attribute::NOMOVE && TW_NOGROUND == Inspect::Attribute::NOGROUND &&
                  TW_WATER == Inspect::Attribute::WATER && TW_ACTION == Inspect::Attribute::ACTION &&
                  TW_HEIGHT == Inspect::Attribute::HEIGHT && TW_CAMERA_UP == Inspect::Attribute::CAMERA_UP &&
                  TW_NOATTACKZONE == Inspect::Attribute::NOATTACK,
              "the inspection units name the engine's TW_* bits");

constexpr int FIRST_WORLD_FOLDER = 1; // Data/World1 is map index 0
constexpr int NO_GATE = 0;
// TerrainHeight.OZB stores one byte per corner as height / factor.
constexpr float HEIGHT_BYTE_MAX = 255.0f;
constexpr float HEIGHT_FACTOR = 1.5f;
constexpr float LOGIN_SCENE_HEIGHT_FACTOR = 3.0f;

// The catalog of one world, read again when the file changes.
struct CachedCatalog
{
    int world = -1;
    std::filesystem::file_time_type written{};
    Editor::Assets::CatalogLoad load;
};

CachedCatalog& CatalogCache()
{
    static CachedCatalog cache;
    return cache;
}

std::filesystem::file_time_type LastWritten(const std::filesystem::path& file)
{
    std::error_code failure;
    const auto written = std::filesystem::last_write_time(file, failure);
    return failure ? std::filesystem::file_time_type{} : written;
}

const Editor::Assets::CatalogLoad& LoadedCatalog(int world)
{
    CachedCatalog& cache = CatalogCache();
    const std::filesystem::path& repo = Editor::Files::RepoRoot().root;
    const std::filesystem::file_time_type written = LastWritten(Editor::Assets::CatalogFile(repo, world));
    if (cache.world != world || cache.written != written)
    {
        cache.world = world;
        cache.written = written;
        cache.load = repo.empty() ? Editor::Assets::CatalogLoad{} : Editor::Assets::LoadCatalog(repo, world);
    }
    return cache.load;
}

std::string ObjectName(const Editor::Assets::CatalogLoad& catalog, int type)
{
    const Editor::Assets::CatalogModel* model = catalog.catalog ? catalog.catalog->FindByType(type) : nullptr;
    return model != nullptr ? model->name : Editor::ObjectPlace::ModelName(type);
}

Inspect::MapObjectRecord RecordOf(const OBJECT& object, const Editor::Assets::CatalogLoad& catalog)
{
    Inspect::MapObjectRecord record;
    record.loadedRecord = object.SaveOrder;
    record.type = object.Type;
    record.name = ObjectName(catalog, object.Type);
    VectorCopy(object.Position, record.position);
    VectorCopy(object.Angle, record.angle);
    record.scale = object.Scale;
    record.block = object.Block;
    record.groundHeight = RequestTerrainHeight(object.Position[0], object.Position[1]);
    return record;
}

struct SavedStateTracker
{
    Inspect::SavedMapState state;
    unsigned int generation = 0;
};

SavedStateTracker& Tracker()
{
    static SavedStateTracker tracker;
    return tracker;
}

Inspect::MapDigests CurrentDigests()
{
    return Inspect::DigestMap(Terrain(), Objects());
}
} // namespace

int WorldFolder()
{
    return gMapManager.WorldActive + FIRST_WORLD_FOLDER;
}

Inspect::MapIdentity Identity()
{
    Inspect::MapIdentity identity;
    identity.map = gMapManager.WorldActive;
    identity.world = WorldFolder();
    identity.name = Core::Text::ToUtf8(gMapManager.GetMapName(gMapManager.WorldActive));
    return identity;
}

Inspect::TerrainView Terrain()
{
    Inspect::TerrainView terrain;
    terrain.baseTiles = TerrainMappingLayer1;
    terrain.overlayTiles = TerrainMappingLayer2;
    terrain.overlayAlpha = TerrainMappingAlpha;
    terrain.height = BackTerrainHeight;
    terrain.attribute = reinterpret_cast<const std::uint16_t*>(TerrainWall);
    terrain.light = &TerrainLight[0][0];
    return terrain;
}

std::vector<Inspect::MapObjectRecord> Objects()
{
    const Editor::Assets::CatalogLoad& catalog = LoadedCatalog(WorldFolder());
    std::vector<Inspect::MapObjectRecord> records;
    Editor::ObjectPlace::ForEachLiveObject(
        [&records, &catalog](OBJECT* object)
        {
            if (IsSavedWorldObject(object))
                records.push_back(RecordOf(*object, catalog));
        });
    Inspect::SortInSaveOrder(records);
    return records;
}

std::vector<Inspect::TileSlotInfo> TileSlots()
{
    std::vector<Inspect::TileSlotInfo> slots;
    for (int slot = 0; slot < Inspect::TILE_SLOT_COUNT; ++slot)
    {
        const BITMAP_t* bitmap = Bitmaps.FindTexture(BITMAP_MAPTILE + slot);
        Inspect::TileSlotInfo info;
        info.slot = slot;
        if (bitmap != nullptr)
            info.file = Core::Text::ToUtf8(bitmap->FileName);
        slots.push_back(std::move(info));
    }
    return slots;
}

std::vector<Gate> Gates()
{
    std::vector<Gate> gates;
    if (GateAttribute == nullptr)
        return gates;
    for (int number = 0; number < MAX_GATES; ++number)
    {
        const GATE_ATTRIBUTE& attribute = GateAttribute[number];
        if (attribute.Flag == NO_GATE || attribute.Map != gMapManager.WorldActive)
            continue;
        Gate gate;
        gate.number = number;
        gate.kind = attribute.Flag;
        gate.x1 = attribute.x1;
        gate.y1 = attribute.y1;
        gate.x2 = attribute.x2;
        gate.y2 = attribute.y2;
        gate.target = attribute.Target;
        gate.targetMap = attribute.Target < MAX_GATES ? GateAttribute[attribute.Target].Map : -1;
        gate.level = attribute.Level;
        gates.push_back(gate);
    }
    return gates;
}

CatalogStatus Catalog()
{
    CatalogStatus status;
    const int world = WorldFolder();
    const std::filesystem::path& repo = Editor::Files::RepoRoot().root;
    if (repo.empty())
    {
        status.problem = Editor::Files::RepoRoot().description;
        return status;
    }
    status.file = Editor::Assets::CatalogFile(repo, world);
    const Editor::Assets::CatalogLoad& load = LoadedCatalog(world);
    status.available = load.catalog.has_value();
    status.models = load.catalog ? static_cast<int>(load.catalog->models.size()) : 0;
    if (!load.fileFound)
        status.problem = "there is no catalog for World" + std::to_string(world);
    else if (!status.available)
        status.problem = load.error;
    return status;
}

void SyncWithLoadedMap()
{
    SavedStateTracker& tracker = Tracker();
    const unsigned int generation = ObjectListGeneration();
    if (tracker.state.IsKnown() && tracker.generation == generation)
        return;
    tracker.generation = generation;
    tracker.state.Reset(CurrentDigests());
}

void NoteSaved(Inspect::SaveUnit unit, int world)
{
    if (world != WorldFolder())
        return;
    SyncWithLoadedMap();
    Tracker().state.MarkSaved(unit, CurrentDigests()[static_cast<std::size_t>(unit)]);
}

std::array<bool, Inspect::SAVE_UNIT_COUNT> UnsavedUnits()
{
    SyncWithLoadedMap();
    return Tracker().state.Unsaved(CurrentDigests());
}

float GroundHeight(float x, float y)
{
    return RequestTerrainHeight(x, y);
}

float HeightFactor()
{
    return gMapManager.WorldActive == WD_55LOGINSCENE ? LOGIN_SCENE_HEIGHT_FACTOR : HEIGHT_FACTOR;
}

float MaxStoredHeight()
{
    return HEIGHT_BYTE_MAX * HeightFactor();
}

std::string CatalogName(int type)
{
    const Editor::Assets::CatalogLoad& catalog = LoadedCatalog(WorldFolder());
    const Editor::Assets::CatalogModel* model = catalog.catalog ? catalog.catalog->FindByType(type) : nullptr;
    return model != nullptr ? model->name : std::string();
}
} // namespace Editor::LiveMap

#endif // _EDITOR
