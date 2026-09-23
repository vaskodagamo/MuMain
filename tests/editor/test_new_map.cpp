#include "App/stdafx.h"

#include <doctest.h>

#include "TempTree.h"

#include "Assets/TerrainHeightFile.h"
#include "Assets/TerrainLightFile.h"
#include "Core/Globals/_crypt.h" // BuxConvert
#include "Engine/Object/WorldObjectFile.h"
#include "MapScript/AttributeRules.h"
#include "NewMap/EncTerrainFile.h"
#include "NewMap/NamedModels.h"
#include "NewMap/NewMapPlan.h"
#include "NewMap/NewMapWriter.h"
#include "NewMap/WorldFolders.h"
#include "Render/Terrain/TerrainFiles.h"
#include "Render/Terrain/ZzzLodTerrain.h" // MapFileEncrypt / MapFileDecrypt

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace Editor::NewMap;
namespace fs = std::filesystem;

namespace
{
constexpr int NEW_MAP = 82;
constexpr int NEW_WORLD = 83;
constexpr int LORENCIA_WORLD = 1;
constexpr std::size_t LORENCIA_NAMED_MODELS = 112; // Object1's 115 models minus bird, butterfly and fish
constexpr float HEIGHT_FACTOR = 1.5f;
constexpr float JPEG_TOLERANCE = 3.0f / 255.0f;

MapFileCodec EngineCodec()
{
    MapFileCodec codec;
    codec.encrypt = [](const Bytes& plain)
    {
        Bytes source = plain;
        Bytes file(plain.size());
        MapFileEncrypt(file.data(), source.data(), static_cast<int>(source.size()));
        return file;
    };
    codec.decrypt = [](const Bytes& file)
    {
        Bytes source = file;
        Bytes plain(file.size());
        MapFileDecrypt(plain.data(), source.data(), static_cast<int>(source.size()));
        return plain;
    };
    codec.buxConvert = [](Bytes& bytes) { BuxConvert(bytes.data(), static_cast<int>(bytes.size())); };
    return codec;
}

fs::path ShippedGameRoot()
{
    return fs::path(MU_REPO_ROOT) / "src" / "bin";
}

Bytes ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

const PlannedFile* FindPlanned(const NewMapPlan& plan, const std::string& relative)
{
    const auto found = std::find_if(plan.files.begin(), plan.files.end(), [&relative](const PlannedFile& file)
                                    { return file.relative.generic_string() == relative; });
    return found != plan.files.end() ? &*found : nullptr;
}

NewMapRequest LorenciaCopy()
{
    NewMapRequest request;
    request.map = NEW_MAP;
    request.name = "Lorencia Outskirts";
    request.source = MapSource::Template;
    request.templateWorld = LORENCIA_WORLD;
    request.modelsWorld = LORENCIA_WORLD;
    return request;
}

NewMapRequest FlatGrass()
{
    NewMapRequest request;
    request.map = NEW_MAP;
    request.name = "Flat test";
    request.source = MapSource::Blank;
    request.blank.heightByte = 100;
    request.blank.tileSlot = 0; // TileGrass01
    request.blank.attribute = Editor::MapScript::Attributes::WALKABLE;
    request.blank.light = {0.5f, 0.6f, 0.7f};
    request.texturesWorld = LORENCIA_WORLD;
    return request;
}

// A folder that looks like a checkout to the repository mirror.
fs::path MakeRepo(const fs::path& root)
{
    fs::create_directories(root / "src" / "bin" / "Data");
    fs::create_directories(root / ".git");
    return root;
}
} // namespace

TEST_CASE("New map: the numbers a new map may take")
{
    std::string error;
    CHECK_FALSE(CheckNewMapNumber(81, error));
    CHECK(error.find("82 to 254") != std::string::npos);
    CHECK(CheckNewMapNumber(82, error));
    CHECK(CheckNewMapNumber(254, error));
    CHECK_FALSE(CheckNewMapNumber(255, error));
    CHECK_FALSE(CheckNewMapNumber(54, error));
}

TEST_CASE("New map: Lorencia's model table names files that exist")
{
    std::size_t models = 0;
    for (const NamedModelRun& run : LorenciaModels())
    {
        for (int index = 0; index < run.count; ++index)
        {
            CHECK(run.firstType + index < MODEL_TYPE_LIMIT);
            CHECK(fs::exists(ShippedGameRoot() / "Data" / "Object1" / NamedModelFile(run, index)));
            ++models;
        }
    }
    CHECK(models == LORENCIA_NAMED_MODELS);
    CHECK(GenericModelFile(0) == "Object01.bmd");
    CHECK(GenericModelFile(159) == "Object160.bmd");
}

TEST_CASE("New map: Lorencia's model table uses the engine's model numbers")
{
    // CMapManager::LoadWorld's order, and the MODEL_* numbers it loads each run into.
    const std::vector<int> engine = {MODEL_TREE01,         MODEL_GRASS01,
                                     MODEL_STONE01,        MODEL_STONE_STATUE01,
                                     MODEL_STEEL_STATUE,   MODEL_TOMB01,
                                     MODEL_FIRE_LIGHT01,   MODEL_BONFIRE,
                                     MODEL_DUNGEON_GATE,   MODEL_TREASURE_DRUM,
                                     MODEL_TREASURE_CHEST, MODEL_SHIP,
                                     MODEL_STONE_WALL01,   MODEL_MU_WALL01,
                                     MODEL_STEEL_WALL01,   MODEL_STEEL_DOOR,
                                     MODEL_CANNON01,       MODEL_BRIDGE,
                                     MODEL_FENCE01,        MODEL_BRIDGE_STONE,
                                     MODEL_STREET_LIGHT,   MODEL_CURTAIN,
                                     MODEL_CARRIAGE01,     MODEL_STRAW01,
                                     MODEL_SIGN01,         MODEL_MERCHANT_ANIMAL01,
                                     MODEL_WATERSPOUT,     MODEL_WELL01,
                                     MODEL_HANGING,        MODEL_HOUSE01,
                                     MODEL_TENT,           MODEL_STAIR,
                                     MODEL_HOUSE_WALL01,   MODEL_HOUSE_ETC01,
                                     MODEL_LIGHT01,        MODEL_POSE_BOX,
                                     MODEL_FURNITURE01,    MODEL_CANDLE,
                                     MODEL_BEER01};
    REQUIRE(LorenciaModels().size() == engine.size());
    for (std::size_t run = 0; run < engine.size(); ++run)
    {
        CAPTURE(LorenciaModels()[run].baseName);
        CHECK(LorenciaModels()[run].firstType == engine[run]);
    }
    CHECK(MODEL_TYPE_LIMIT == MAX_WORLD_OBJECTS);
    CHECK(FIRST_MODEL_TYPE == MODEL_WORLD_OBJECT);
}

TEST_CASE("New map: a copy of Lorencia is renumbered and its models renamed")
{
    const MapFileCodec codec = EngineCodec();
    NewMapPlan plan;
    std::string error;
    REQUIRE_MESSAGE(PlanNewMap(ShippedGameRoot(), LorenciaCopy(), codec, plan, error), error);
    CHECK(plan.map == NEW_MAP);
    CHECK(plan.world == NEW_WORLD);

    const fs::path lorencia = ShippedGameRoot() / "Data" / "World1";
    for (const EncTerrainKind kind : {EncTerrainKind::Mapping, EncTerrainKind::Attribute, EncTerrainKind::Objects})
    {
        const std::string extension = Extension(kind);
        const PlannedFile* copy = FindPlanned(plan, "Data/World83/EncTerrain83" + extension);
        REQUIRE(copy != nullptr);
        Bytes plain = DecodeEncTerrain(kind, copy->bytes, codec);
        Bytes original = DecodeEncTerrain(kind, ReadFile(lorencia / ("EncTerrain1" + extension)), codec);
        CHECK(plain[MAP_NUMBER_OFFSET] == NEW_WORLD);
        CHECK(original[MAP_NUMBER_OFFSET] == LORENCIA_WORLD);
        plain[MAP_NUMBER_OFFSET] = LORENCIA_WORLD;
        CHECK(plain == original); // only the folder number changed
    }
    CHECK(FindPlanned(plan, "Data/World83/TerrainHeight.OZB")->bytes == ReadFile(lorencia / "TerrainHeight.OZB"));
    CHECK(FindPlanned(plan, "Data/World83/TerrainLight.OZJ")->bytes == ReadFile(lorencia / "TerrainLight.OZJ"));
    CHECK(FindPlanned(plan, "Data/World83/TileGrass01.OZJ") != nullptr);
    CHECK(FindPlanned(plan, "Data/World83/mini_map.OZT") != nullptr);
    CHECK(FindPlanned(plan, "Data/World83/MapName.txt")->bytes ==
          Bytes{'L', 'o', 'r', 'e', 'n', 'c', 'i', 'a', ' ', 'O', 'u', 't', 's', 'k', 'i', 'r', 't', 's', '\n'});
    CHECK(FindPlanned(plan, "Data/World83/EncTerrain1.att1") == nullptr);

    const fs::path object1 = ShippedGameRoot() / "Data" / "Object1";
    CHECK(FindPlanned(plan, "Data/Object83/Object01.bmd")->bytes == ReadFile(object1 / "Tree01.bmd"));
    CHECK(FindPlanned(plan, "Data/Object83/Object116.bmd")->bytes == ReadFile(object1 / "House01.bmd"));
    CHECK(FindPlanned(plan, "Data/Object83/Bird01.bmd") == nullptr);
    const std::size_t models = static_cast<std::size_t>(
        std::count_if(plan.files.begin(), plan.files.end(),
                      [](const PlannedFile& file)
                      {
                          return file.relative.parent_path().generic_string() == "Data/Object83" &&
                                 file.relative.extension() == ".bmd";
                      }));
    CHECK(models == LORENCIA_NAMED_MODELS);
    CHECK_FALSE(plan.warnings.empty()); // what stays tied to Lorencia's map number
}

TEST_CASE("New map: a flat map decodes as the client loads it")
{
    const MapFileCodec codec = EngineCodec();
    NewMapPlan plan;
    std::string error;
    REQUIRE_MESSAGE(PlanNewMap(ShippedGameRoot(), FlatGrass(), codec, plan, error), error);

    const Bytes mapping =
        DecodeEncTerrain(EncTerrainKind::Mapping, FindPlanned(plan, "Data/World83/EncTerrain83.map")->bytes, codec);
    std::vector<std::uint8_t> layer1(Render::Terrain::Files::CELLS);
    std::vector<std::uint8_t> layer2(Render::Terrain::Files::CELLS);
    std::vector<float> alpha(Render::Terrain::Files::CELLS);
    CHECK(Render::Terrain::Files::DecodeMapping(mapping.data(), mapping.size(),
                                                {layer1.data(), layer2.data(), alpha.data()}, error) == NEW_WORLD);
    CHECK(std::all_of(layer1.begin(), layer1.end(), [](std::uint8_t slot) { return slot == 0; }));
    CHECK(std::all_of(layer2.begin(), layer2.end(), [](std::uint8_t slot) { return slot == 255; }));

    const Bytes walls =
        DecodeEncTerrain(EncTerrainKind::Attribute, FindPlanned(plan, "Data/World83/EncTerrain83.att")->bytes, codec);
    std::vector<std::uint16_t> tiles;
    CHECK(Editor::MapScript::Attributes::DecodeClientFile(walls.data(), walls.size(), NEW_MAP, tiles, error));
    CHECK(walls[MAP_NUMBER_OFFSET] == NEW_WORLD);

    const Bytes objects =
        DecodeEncTerrain(EncTerrainKind::Objects, FindPlanned(plan, "Data/World83/EncTerrain83.obj")->bytes, codec);
    Engine::Object::WorldObjectFile::Contents contents;
    CHECK(Engine::Object::WorldObjectFile::Decode(objects.data(), objects.size(), contents));
    CHECK(contents.mapNumber == NEW_WORLD);
    CHECK(contents.records.empty());

    std::vector<float> heights(Render::Terrain::Files::CELLS);
    CHECK(Editor::HeightMap::DecodeOzb(FindPlanned(plan, "Data/World83/TerrainHeight.OZB")->bytes, HEIGHT_FACTOR,
                                       heights.data(), error));
    CHECK(std::all_of(heights.begin(), heights.end(), [](float height) { return height == 100 * HEIGHT_FACTOR; }));

    std::vector<float> light(Render::Terrain::Files::CELLS * 3);
    CHECK(Editor::LightMap::DecodeOzj(FindPlanned(plan, "Data/World83/TerrainLight.OZJ")->bytes,
                                      Editor::LightMap::LIGHT_MAP_SIZE, light.data(), error));
    CHECK(std::fabs(light[0] - 0.5f) < JPEG_TOLERANCE);
    CHECK(std::fabs(light[1] - 0.6f) < JPEG_TOLERANCE);
    CHECK(std::fabs(light[2] - 0.7f) < JPEG_TOLERANCE);

    CHECK(FindPlanned(plan, "Data/World83/TileGrass01.OZJ") != nullptr);
    CHECK(FindPlanned(plan, "Data/World83/mini_map.OZT") == nullptr);
    CHECK(FindPlanned(plan, "Data/World83/TerrainLight1.OZJ") == nullptr);
}

TEST_CASE("New map: requests the client could not load are refused")
{
    const MapFileCodec codec = EngineCodec();
    NewMapPlan plan;
    std::string error;

    NewMapRequest doppelganger = LorenciaCopy();
    doppelganger.templateWorld = 67; // Doppelganger 2 stores 24 bits a corner
    CHECK_FALSE(PlanNewMap(ShippedGameRoot(), doppelganger, codec, plan, error));
    CHECK(error.find("24 bits") != std::string::npos);

    NewMapRequest missing = LorenciaCopy();
    missing.templateWorld = 200;
    CHECK_FALSE(PlanNewMap(ShippedGameRoot(), missing, codec, plan, error));

    NewMapRequest noTexture = FlatGrass();
    noTexture.blank.tileSlot = 14; // ExtTile01: Lorencia has none
    CHECK_FALSE(PlanNewMap(ShippedGameRoot(), noTexture, codec, plan, error));
    CHECK(error.find("ExtTile01") != std::string::npos);

    NewMapRequest badWalk = FlatGrass();
    badWalk.blank.attribute = 5;
    CHECK_FALSE(PlanNewMap(ShippedGameRoot(), badWalk, codec, plan, error));

    NewMapRequest noName = FlatGrass();
    noName.name = "  ";
    CHECK_FALSE(PlanNewMap(ShippedGameRoot(), noName, codec, plan, error));
    noName.name = "two\nlines";
    CHECK_FALSE(PlanNewMap(ShippedGameRoot(), noName, codec, plan, error));
    noName.name = std::string(65, 'x');
    CHECK_FALSE(PlanNewMap(ShippedGameRoot(), noName, codec, plan, error));

    NewMapRequest oldNumber = FlatGrass();
    oldNumber.map = 81;
    CHECK_FALSE(PlanNewMap(ShippedGameRoot(), oldNumber, codec, plan, error));
}

TEST_CASE("New map: written into the game and the repository, never over existing folders")
{
    EditorTest::TempTree temp("mu_new_map_write");
    const fs::path game = temp.Root() / "game";
    const fs::path repo = MakeRepo(temp.Root() / "repo");
    const MapFileCodec codec = EngineCodec();
    NewMapPlan plan;
    std::string error;
    REQUIRE(PlanNewMap(ShippedGameRoot(), FlatGrass(), codec, plan, error));

    std::vector<WrittenFile> written;
    REQUIRE_MESSAGE(WriteNewMap(game, repo, "20260923-120000", plan, written, error), error);
    CHECK(written.size() == plan.files.size());
    CHECK(fs::is_directory(game / "Data" / "Object83"));
    for (const WrittenFile& file : written)
    {
        CHECK(file.repo.result == Editor::Files::RepoCopyResult::Created);
        CHECK(fs::exists(file.runtime));
        CHECK(ReadFile(file.runtime) == ReadFile(repo / "src" / "bin" / file.relative));
    }

    CHECK_FALSE(WriteNewMap(game, repo, "20260923-120001", plan, written, error));
    CHECK(error.find("already exists in the game's Data folder") != std::string::npos);

    const fs::path otherGame = temp.Root() / "other";
    CHECK_FALSE(WriteNewMap(otherGame, repo, "20260923-120002", plan, written, error));
    CHECK(error.find("repository") != std::string::npos);
    CHECK_FALSE(fs::exists(otherGame / "Data" / "World83"));
}

TEST_CASE("New map: map folders and the next free number")
{
    EditorTest::TempTree temp("mu_new_map_folders");
    fs::create_directories(temp.Root() / "Data" / "World1");
    fs::create_directories(temp.Root() / "Data" / "World83");
    fs::create_directories(temp.Root() / "Data" / "Object84");
    CHECK(ExistingWorldFolders(temp.Root()) == std::vector<int>{1, 83});
    CHECK(NextFreeMapNumber(temp.Root(), {}) == 84);

    // The checkout may already hold new maps; the next free one has neither folder.
    const std::vector<int> shipped = ExistingWorldFolders(ShippedGameRoot());
    CHECK(std::find(shipped.begin(), shipped.end(), 82) != shipped.end()); // Karutan 2
    const int next = NextFreeMapNumber(ShippedGameRoot(), {});
    REQUIRE(next >= NEW_MAP);
    CHECK_FALSE(fs::exists(ShippedGameRoot() / WorldFolder(next + 1)));
    CHECK_FALSE(fs::exists(ShippedGameRoot() / ObjectFolder(next + 1)));
}
