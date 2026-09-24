// doctest unit tests for the Map Editor's read-only map inspection (MapInspect/): the
// layer images, legends and numbers a scripted client reads a map through, the camera
// framing math and the unsaved-edit fingerprints. No engine state: the tests build
// their own 256 x 256 terrain.
//
// Run: ctest --test-dir <build directory> --build-config Release -R "\[map-inspect\]"

#include <doctest.h>

#include "MapInspect/AreaStats.h"
#include "MapInspect/AttributeBits.h"
#include "MapInspect/AttributePalette.h"
#include "MapInspect/CameraFraming.h"
#include "MapInspect/LayerImage.h"
#include "MapInspect/MapDigest.h"
#include "MapInspect/MapExport.h"
#include "MapInspect/MapObjectList.h"
#include "MapInspect/PngFile.h"
#include "MapInspect/SavedMapState.h"
#include "MapInspect/TilePalette.h"

#include "Engine/Object/WorldObjectFile.h"

#include "TempTree.h"

#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace Editor::MapInspect;

namespace
{
constexpr std::size_t CELLS = MAP_CELLS;

// A flat, walkable map on texture slot 0 with no overlay, lit fully.
struct TestTerrain
{
    TestTerrain()
        : base(CELLS, 0), overlay(CELLS, NO_OVERLAY_TILE), alpha(CELLS, 0.0f), height(CELLS, 0.0f), attribute(CELLS, 0),
          light(CELLS * 3, 1.0f)
    {
    }

    TerrainView View() const
    {
        return TerrainView{base.data(), overlay.data(), alpha.data(), height.data(), attribute.data(), light.data()};
    }

    std::vector<std::uint8_t> base;
    std::vector<std::uint8_t> overlay;
    std::vector<float> alpha;
    std::vector<float> height;
    std::vector<std::uint16_t> attribute;
    std::vector<float> light;
};

MapObjectRecord Object(int type, float x, float y, int loadedRecord)
{
    MapObjectRecord object;
    object.type = type;
    object.position[0] = x;
    object.position[1] = y;
    object.loadedRecord = loadedRecord;
    return object;
}

std::uint8_t GreyAt(const Image& image, int column, int row)
{
    return image.pixels[static_cast<std::size_t>(row * image.width + column) * image.channels];
}

Rgb RgbAt(const Image& image, int column, int row)
{
    const std::size_t offset = static_cast<std::size_t>(row * image.width + column) * image.channels;
    return Rgb{image.pixels[offset], image.pixels[offset + 1], image.pixels[offset + 2]};
}

bool SameColor(const Rgb& a, const Rgb& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

std::uint32_t BigEndian32(const std::string& bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value = (value << 8) | static_cast<std::uint8_t>(bytes[offset + i]);
    return value;
}
} // namespace

TEST_CASE("Rectangles are read in any corner order and refused off the map [editor][map-inspect]")
{
    CellRect area;
    std::string error;
    REQUIRE(AreaFromCorners(20, 30, 10, 5, area, error));
    CHECK(area.minX == 10);
    CHECK(area.minY == 5);
    CHECK(area.maxX == 20);
    CHECK(area.maxY == 30);
    CHECK(CellCount(area) == 11 * 26);

    CHECK_FALSE(AreaFromCorners(0, 0, MAP_TILES, 3, area, error));
    CHECK(error.find("outside the map") != std::string::npos);
    CHECK_FALSE(AreaFromCorners(-1, 0, 3, 3, area, error));

    CHECK(TileOf(0.0f) == 0);
    CHECK(TileOf(199.9f) == 1);
    CHECK(TileOf(-50.0f) == 0);
    CHECK(TileOf(1.0e6f) == MAP_TILES - 1);
    CHECK(TileCentre(3) == doctest::Approx(350.0f));
}

TEST_CASE("Layer images put north up: row 0 is the area's highest y [editor][map-inspect]")
{
    TestTerrain terrain;
    const CellRect area{10, 20, 13, 22};        // 4 x 3 tiles
    terrain.height[CellIndex(10, 22)] = 100.0f; // north-west corner of the area
    terrain.height[CellIndex(13, 20)] = -50.0f; // south-east corner

    const HeightRange range = MeasureHeightRange(terrain.View(), area);
    CHECK(range.min == -50.0f);
    CHECK(range.max == 100.0f);
    const Image image = RenderTerrainLayer(terrain.View(), MapLayer::Height, area, range);
    REQUIRE(image.width == 4);
    REQUIRE(image.height == 3);
    REQUIRE(image.channels == 1);
    CHECK(GreyAt(image, 0, 0) == 255); // (10, 22)
    CHECK(GreyAt(image, 3, 2) == 0);   // (13, 20)
    CHECK(GreyAt(image, 1, 1) == HeightLevel(0.0f, range));

    int x = 0;
    int y = 0;
    TileOfPixel(area, 3, 0, x, y);
    CHECK(x == 13);
    CHECK(y == 22);
    int column = 0;
    int row = 0;
    PixelOfTile(area, 10, 20, column, row);
    CHECK(column == 0);
    CHECK(row == 2);
}

TEST_CASE("A flat area has one grey level and a zero height step [editor][map-inspect]")
{
    TestTerrain terrain;
    const HeightRange range = MeasureHeightRange(terrain.View(), WholeMap());
    CHECK(range.UnitsPerLevel() == 0.0f);
    CHECK(HeightLevel(0.0f, range) == 0);
}

TEST_CASE("Attribute colours follow the strongest flag and ignore the character bit [editor][map-inspect]")
{
    CHECK(SameColor(AttributeColor(0), AttributeColor(Attribute::CHARACTER)));
    CHECK(StoredAttribute(Attribute::SAFEZONE | Attribute::CHARACTER) == Attribute::SAFEZONE);
    CHECK(StoredAttribute(Attribute::NOATTACK) == 0); // the .att keeps the low byte only

    const Rgb walkable = AttributeColor(0);
    const Rgb safe = AttributeColor(Attribute::SAFEZONE);
    const Rgb blocked = AttributeColor(Attribute::NOMOVE);
    const Rgb townWall = AttributeColor(Attribute::SAFEZONE | Attribute::NOMOVE);
    const Rgb voidTile = AttributeColor(Attribute::NOGROUND | Attribute::NOMOVE | Attribute::SAFEZONE);
    CHECK_FALSE(SameColor(walkable, safe));
    CHECK_FALSE(SameColor(safe, blocked));
    CHECK_FALSE(SameColor(blocked, townWall));
    CHECK(SameColor(voidTile, Rgb{0, 0, 0}));

    const std::vector<std::string> names = AttributeFlagNames(Attribute::SAFEZONE | Attribute::NOMOVE);
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "safezone");
    CHECK(names[1] == "nomove");
}

TEST_CASE("Texture slots are named like the loader names their files [editor][map-inspect]")
{
    CHECK(TileSlotName(0) == "TileGrass01");
    CHECK(TileSlotName(5) == "TileWater01");
    CHECK(TileSlotName(13) == "TileRock07");
    CHECK(TileSlotName(14) == "ExtTile01");
    CHECK(TileSlotName(29) == "ExtTile16");
    CHECK(TileSlotName(NO_OVERLAY_TILE) == "none");

    // Every slot a map can load has a colour of its own.
    for (int a = 0; a < TILE_SLOT_COUNT; ++a)
    {
        for (int b = a + 1; b < TILE_SLOT_COUNT; ++b)
            CHECK_FALSE(SameColor(TileSlotColor(a), TileSlotColor(b)));
    }
}

TEST_CASE("Area numbers count every cell of the rectangle [editor][map-inspect]")
{
    TestTerrain terrain;
    terrain.height[CellIndex(1, 1)] = 30.0f;
    terrain.attribute[CellIndex(0, 0)] = Attribute::SAFEZONE;
    terrain.attribute[CellIndex(1, 0)] = Attribute::SAFEZONE | Attribute::CHARACTER;
    terrain.base[CellIndex(1, 1)] = 5;
    terrain.overlay[CellIndex(0, 1)] = 7;
    terrain.alpha[CellIndex(0, 1)] = 1.0f;

    const AreaStats stats = MeasureArea(terrain.View(), CellRect{0, 0, 1, 1});
    CHECK(stats.cells == 4);
    CHECK(stats.height.min == 0.0f);
    CHECK(stats.height.max == 30.0f);
    CHECK(stats.height.mean == doctest::Approx(7.5f));
    CHECK(stats.attributes.at(Attribute::SAFEZONE) == 2);
    CHECK(stats.attributes.at(0) == 2);
    CHECK(stats.baseTiles.at(0) == 3);
    CHECK(stats.baseTiles.at(5) == 1);
    CHECK(stats.overlayTiles.at(7) == 1);
    CHECK(stats.overlayTiles.at(NO_OVERLAY_TILE) == 3);
    CHECK(stats.overlayAlphaMean == doctest::Approx(0.25f));
    CHECK(stats.lightMean[1] == doctest::Approx(1.0f));
}

TEST_CASE("Objects sort into the order a save writes them [editor][map-inspect]")
{
    std::vector<MapObjectRecord> objects = {
        Object(1, 100.0f, 100.0f, NOT_FROM_FILE), Object(2, 200.0f, 200.0f, 5), Object(3, 300.0f, 300.0f, 0),
        Object(4, 400.0f, 400.0f, NOT_FROM_FILE), Object(5, 500.0f, 500.0f, 2),
    };

    // The engine's own rule, applied to the same records.
    std::vector<Engine::Object::WorldObjectFile::OrderedRecord> ordered;
    for (const MapObjectRecord& object : objects)
    {
        Engine::Object::WorldObjectFile::OrderedRecord record;
        record.order = object.loadedRecord;
        record.record.type = static_cast<std::int16_t>(object.type);
        ordered.push_back(record);
    }
    const auto saved = Engine::Object::WorldObjectFile::InSaveOrder(ordered);

    SortInSaveOrder(objects);
    REQUIRE(objects.size() == saved.size());
    for (std::size_t i = 0; i < objects.size(); ++i)
        CHECK(objects[i].type == saved[i].type);
    CHECK(objects[0].type == 3); // record 0 first
    CHECK(objects[3].type == 1); // added objects keep their order, after the file's
    CHECK(objects[4].type == 4);
}

TEST_CASE("Objects are found by the tile they stand on [editor][map-inspect]")
{
    const std::vector<MapObjectRecord> objects = {
        Object(1, 1050.0f, 2050.0f, 0), // tile (10, 20)
        Object(2, 1150.0f, 2050.0f, 1), // tile (11, 20)
        Object(3, 1050.0f, 2050.0f, 2), // tile (10, 20) again
    };
    const std::vector<std::size_t> inside = ObjectsInside(objects, CellRect{10, 20, 10, 21});
    REQUIRE(inside.size() == 2);
    CHECK(inside[0] == 0);
    CHECK(inside[1] == 2);

    const nlohmann::json entry = ObjectToJson(objects[1], 1);
    CHECK(entry["index"] == 1);
    CHECK(entry["tile"] == nlohmann::json::array({11, 20}));
    CHECK(entry["loaded_record"] == 1);

    const Image occupancy = RenderObjectOccupancy(objects, CellRect{10, 20, 11, 20});
    REQUIRE(occupancy.width == 2);
    CHECK(GreyAt(occupancy, 0, 0) == OccupancyLevel(2));
    CHECK(GreyAt(occupancy, 1, 0) == OccupancyLevel(1));
    CHECK(OccupancyLevel(0) == 0);
    CHECK(OccupancyLevel(2) > OccupancyLevel(1));
    CHECK(OccupancyLevel(100) == 255);
}

TEST_CASE("A top-down framing fits the whole rectangle into the view [editor][map-inspect]")
{
    const ViewShape view{40.0f, 16.0f / 9.0f};
    const CellRect area{0, 0, 99, 49}; // 10000 x 5000 world units
    const float height = TopDownHeight(area, view);
    const float tanVertical = std::tan(view.verticalFovDegrees * 0.5f * 3.14159265f / 180.0f);
    const float visibleHalfHeight = height * tanVertical;
    const float visibleHalfWidth = visibleHalfHeight * view.aspect;
    CHECK(visibleHalfWidth >= 5000.0f);
    CHECK(visibleHalfHeight >= 2500.0f);
    // Tight on the limiting side: only the margin is left over.
    CHECK(visibleHalfWidth == doctest::Approx(5000.0f * FRAME_MARGIN).epsilon(0.001));
    CHECK(TopDownViewRange(area, height) > height);

    CHECK(EnginePitch(90.0f) == 0.0f);
    CHECK(EnginePitch(0.0f) == -90.0f);
    CHECK(PitchBelowHorizon(-45.0f) == 45.0f);
    CHECK(NormalizedYaw(-45.0f) == 315.0f);
    CHECK(DistanceForHeight(1000.0f, 90.0f) == doctest::Approx(1000.0f));
    CHECK(DistanceForHeight(1000.0f, 30.0f) == doctest::Approx(2000.0f));
}

TEST_CASE("A rectangle's outline and screen box [editor][map-inspect]")
{
    const std::vector<GroundPoint> outline = AreaOutline(CellRect{0, 0, 15, 7}, 8);
    const auto has = [&outline](float x, float y)
    {
        return std::any_of(outline.begin(), outline.end(),
                           [x, y](const GroundPoint& point) { return point.x == x && point.y == y; });
    };
    CHECK(has(0.0f, 0.0f));
    CHECK(has(1600.0f, 0.0f));
    CHECK(has(0.0f, 800.0f));
    CHECK(has(1600.0f, 800.0f));
    CHECK(has(800.0f, 0.0f));

    const auto box = BoundingBox({{10.4f, 20.6f}, {100.2f, 50.0f}, {40.0f, 90.9f}}, 1920, 1080);
    REQUIRE(box.has_value());
    CHECK(box->x == 10);
    CHECK(box->y == 20);
    CHECK(box->width == 91);
    CHECK(box->height == 71);
    CHECK(BoundingBox({{-50.0f, -50.0f}, {-10.0f, -10.0f}}, 100, 100) == std::nullopt);
    const auto clipped = BoundingBox({{-50.0f, 10.0f}, {150.0f, 20.0f}}, 100, 100);
    REQUIRE(clipped.has_value());
    CHECK(clipped->x == 0);
    CHECK(clipped->width == 100);
}

TEST_CASE("Unsaved edits show until saved or undone [editor][map-inspect]")
{
    TestTerrain terrain;
    std::vector<MapObjectRecord> objects = {Object(1, 100.0f, 100.0f, 0), Object(2, 900.0f, 100.0f, 1)};
    SavedMapState saved;
    CHECK_FALSE(saved.IsKnown());
    saved.Reset(DigestMap(terrain.View(), objects));

    const auto isUnsaved = [&](SaveUnit unit)
    { return saved.Unsaved(DigestMap(terrain.View(), objects))[static_cast<std::size_t>(unit)]; };

    terrain.height[CellIndex(4, 4)] = 12.0f;
    CHECK(isUnsaved(SaveUnit::Height));
    CHECK_FALSE(isUnsaved(SaveUnit::Texture));
    terrain.height[CellIndex(4, 4)] = 0.0f; // undone
    CHECK_FALSE(isUnsaved(SaveUnit::Height));

    // A character standing on a tile is not an edit of the attribute file.
    terrain.attribute[CellIndex(3, 3)] = Attribute::CHARACTER;
    CHECK_FALSE(isUnsaved(SaveUnit::Attribute));
    terrain.attribute[CellIndex(3, 3)] = Attribute::NOMOVE;
    CHECK(isUnsaved(SaveUnit::Attribute));
    saved.MarkSaved(SaveUnit::Attribute,
                    DigestMap(terrain.View(), objects)[static_cast<std::size_t>(SaveUnit::Attribute)]);
    CHECK_FALSE(isUnsaved(SaveUnit::Attribute));

    terrain.overlay[CellIndex(1, 2)] = 3;
    CHECK(isUnsaved(SaveUnit::Texture));
    terrain.light[5] = 0.5f;
    CHECK(isUnsaved(SaveUnit::Light));

    // Objects listed in another order are the same objects; a moved one is not.
    std::swap(objects[0], objects[1]);
    CHECK_FALSE(isUnsaved(SaveUnit::Objects));
    objects[0].position[0] += 10.0f;
    CHECK(isUnsaved(SaveUnit::Objects));
    CHECK(SaveUnitName(SaveUnit::Objects) == "objects");
}

TEST_CASE("A PNG holds the image's size and pixels [editor][map-inspect]")
{
    EditorTest::TempTree tree("mu-map-inspect-png");
    Image image = MakeImage(3, 2, 3);
    SetRgb(image, 2, 1, Rgb{10, 20, 30});
    const auto file = tree.Root() / "sub" / "image.png";
    std::string error;
    REQUIRE(WritePng(file, image, error));
    const std::string bytes = EditorTest::ReadText(file);
    REQUIRE(bytes.size() > 33);
    CHECK(bytes.substr(1, 3) == "PNG");
    CHECK(BigEndian32(bytes, 16) == 3);               // IHDR width
    CHECK(BigEndian32(bytes, 20) == 2);               // IHDR height
    CHECK(static_cast<std::uint8_t>(bytes[25]) == 2); // colour type RGB

    CHECK_FALSE(WritePng(tree.Root() / "empty.png", Image{}, error));

    // The rows are filtered and deflated: a smooth frame takes a small part of its raw size.
    // (Decoded against Python's zlib for noise, gradients and real 1920 x 1080 frames when
    // the compressor was written; a doctest has no inflater to do that here.)
    constexpr int SIDE = 256;
    Image gradient = MakeImage(SIDE, SIDE, 3);
    for (int y = 0; y < SIDE; ++y)
    {
        for (int x = 0; x < SIDE; ++x)
            SetRgb(gradient, x, y,
                   Rgb{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), static_cast<std::uint8_t>(x ^ y)});
    }
    const auto compressed = tree.Root() / "gradient.png";
    REQUIRE(WritePng(compressed, gradient, error));
    CHECK(std::filesystem::file_size(compressed) < gradient.pixels.size() / 8);

    const Image cropped = Crop(image, 1, 1, 5, 5);
    REQUIRE(cropped.width == 2);
    REQUIRE(cropped.height == 1);
    CHECK(SameColor(RgbAt(cropped, 1, 0), Rgb{10, 20, 30}));
    CHECK(Crop(image, 5, 5, 2, 2).IsEmpty());
}

TEST_CASE("An export writes each layer, the objects and a legend [editor][map-inspect]")
{
    EditorTest::TempTree tree("mu-map-inspect-export");
    TestTerrain terrain;
    terrain.attribute[CellIndex(5, 5)] = Attribute::SAFEZONE;
    terrain.base[CellIndex(6, 5)] = 5;
    const std::vector<MapObjectRecord> objects = {Object(7, 550.0f, 550.0f, 0), Object(8, 5000.0f, 5000.0f, 1)};

    ExportRequest request;
    request.identity = MapIdentity{1, 0, "Lorencia"};
    request.layers.assign(AllMapLayers().begin(), AllMapLayers().end());
    request.area = CellRect{4, 4, 7, 7};
    request.folder = tree.Root() / "out";
    const std::vector<TileSlotInfo> slots = {{0, "World1\\TileGrass01.jpg"}, {5, "World1\\TileWater01.jpg"}};

    ExportResult result;
    std::string error;
    REQUIRE(ExportMap(terrain.View(), objects, slots, request, result, error));
    CHECK(result.files.size() == AllMapLayers().size() + 2); // the images, objects.json, legend.json
    for (MapLayer layer : AllMapLayers())
        CHECK(std::filesystem::exists(request.folder / LayerFileName(layer)));

    const nlohmann::json legend = nlohmann::json::parse(EditorTest::ReadText(request.folder / "legend.json"));
    CHECK(legend["area"] == nlohmann::json::array({4, 4, 7, 7}));
    CHECK(legend["map_name"] == "Lorencia");
    const nlohmann::json& attributeValues = legend["layers"]["attribute"]["values"];
    CHECK(attributeValues.size() == 2);
    const nlohmann::json& slots1 = legend["layers"]["texture1"]["slots"];
    REQUIRE(slots1.size() == 2);
    CHECK(slots1[1]["slot"] == 5);
    CHECK(slots1[1]["name"] == "TileWater01");
    CHECK(slots1[1]["file"] == "World1\\TileWater01.jpg");
    CHECK(slots1[1]["tiles"] == 1);
    CHECK(legend["layers"]["objects"]["objects_in_area"] == 1);

    const nlohmann::json list = nlohmann::json::parse(EditorTest::ReadText(request.folder / "objects.json"));
    REQUIRE(list["objects"].size() == 1);
    CHECK(list["objects"][0]["type"] == 7);
    CHECK(list["objects_on_map"] == 2);

    ExportRequest none = request;
    none.layers.clear();
    CHECK_FALSE(ExportMap(terrain.View(), objects, slots, none, result, error));
}

TEST_CASE("Layer names read back to their layers [editor][map-inspect]")
{
    for (MapLayer layer : AllMapLayers())
    {
        MapLayer parsed{};
        REQUIRE(MapLayerFromName(MapLayerName(layer), parsed));
        CHECK(parsed == layer);
    }
    MapLayer unknown{};
    CHECK_FALSE(MapLayerFromName("walls", unknown));
}
