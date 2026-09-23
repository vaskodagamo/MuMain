#include <doctest.h>

#include "Editing/TerrainBrush.h"
#include "MapScript/AttributeRules.h"
#include "MapScript/EditScript.h"
#include "MapScript/NameResolver.h"
#include "MapScript/PoissonScatter.h"
#include "MapScript/ScriptCost.h"
#include "MapScript/ScriptDiff.h"
#include "MapScript/ScriptMap.h"
#include "MapScript/ScriptParser.h"
#include "MapScript/ScriptRandom.h"
#include "MapScript/ScriptReport.h"
#include "MapScript/ScriptRunner.h"
#include "MapScript/ScriptShape.h"
#include "MapScript/ValueNoise.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace Editor::MapScript;
using Editor::Editing::CellRect;
using nlohmann::json;

namespace
{
constexpr float GROUND = 100.0f;
constexpr int TREE = 5;
constexpr int BUSH = 6;
constexpr int HOUSE = 40;
constexpr int GRASS_SLOT = 0;
constexpr int DIRT_SLOT = 2;
constexpr int LORENCIA = 0;
constexpr int OTHER_MAP = 30;

float HeightAt(const MapState& state, int x, int y)
{
    return state.terrain.height[Editor::MapInspect::CellIndex(x, y)];
}

std::uint16_t AttributeAt(const MapState& state, int x, int y)
{
    return state.terrain.attribute[Editor::MapInspect::CellIndex(x, y)];
}

// A flat grass map at height 100 with a few objects, like a small loaded map.
MapState FlatMap()
{
    MapState state;
    state.terrain = MapTerrain::Filled(GRASS_SLOT, GROUND, 0.5f);
    const float places[][2] = {{50.5f, 50.5f}, {60.5f, 50.5f}, {120.5f, 120.5f}};
    for (int i = 0; i < 3; ++i)
    {
        MapObject object;
        object.key = 1000 + i;
        object.id = i;
        object.state.type = i == 2 ? HOUSE : TREE;
        object.state.position[0] = places[i][0] * TILE_WORLD;
        object.state.position[1] = places[i][1] * TILE_WORLD;
        object.state.position[2] = GROUND;
        state.objects.push_back(object);
    }
    state.nextId = 3;
    return state;
}

MapContext TestContext(int mapIndex = OTHER_MAP)
{
    MapContext context;
    context.mapIndex = mapIndex;
    context.models = {{TREE, "Tree01", "Tree01"}, {BUSH, "Bush01", "Shrub"}, {HOUSE, "House01", ""}};
    context.tileSlots = {{0, "World1\\TileGrass01.jpg"},
                         {1, "World1\\TileGrass02.jpg"},
                         {2, "World1\\TileGround01.jpg"},
                         {5, "World1\\TileWater01.jpg"},
                         {3, ""}};
    context.maxObjects = 100;
    return context;
}

json Script(json ops, const std::string& label = "")
{
    json script = {{"schema", "mu-map-edit/1"}, {"ops", std::move(ops)}};
    if (!label.empty())
        script["label"] = label;
    return script;
}

json Circle(float x, float y, float radius)
{
    return json{{"type", "circle"}, {"center", {x, y}}, {"radius", radius}};
}

// Parses and runs `script` on `state`; the error when either fails.
std::string Run(const json& document, MapState& state, std::vector<OpReport>& reports,
                const MapContext& context = TestContext())
{
    EditScript script;
    std::string error;
    if (!ParseScript(document, script, error))
        return error;
    if (!RunScript(script, context, state, reports, error))
        return error;
    return {};
}

std::string Run(const json& document, MapState& state)
{
    std::vector<OpReport> reports;
    return Run(document, state, reports);
}

std::string ParseError(const json& document)
{
    EditScript script;
    std::string error;
    CHECK_FALSE(ParseScript(document, script, error));
    return error;
}

bool Contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

std::vector<const MapObject*> PlacedBy(const MapState& state, int op)
{
    std::vector<const MapObject*> placed;
    for (const MapObject& object : state.objects)
    {
        if (object.placedByOp == op)
            placed.push_back(&object);
    }
    return placed;
}
} // namespace

TEST_CASE("Shapes measure how deep a point lies inside them [editor][map-script]")
{
    Shape circle;
    circle.center = {100.0f, 100.0f};
    circle.radius = 5.0f;
    CHECK(InsideDistance(circle, {100.0f, 100.0f}) == doctest::Approx(5.0f));
    CHECK(InsideDistance(circle, {103.0f, 104.0f}) == doctest::Approx(0.0f));
    CHECK(InsideDistance(circle, {110.0f, 100.0f}) < 0.0f);

    Shape rect;
    rect.kind = ShapeKind::Rect;
    rect.tiles = CellRect{10, 20, 19, 24}; // 10 x 5 tiles, covering x 10..20, y 20..25
    CHECK(InsideDistance(rect, {15.0f, 22.5f}) == doctest::Approx(2.5f));
    CHECK(InsideDistance(rect, {20.0f, 22.0f}) == doctest::Approx(0.0f));
    CHECK(InsideDistance(rect, {20.5f, 22.0f}) < 0.0f);
    CHECK(InnerRadius(rect) == doctest::Approx(2.5f));

    Shape lShape; // an L: concave corner at (5, 5)
    lShape.kind = ShapeKind::Polygon;
    lShape.points = {{0, 0}, {10, 0}, {10, 5}, {5, 5}, {5, 10}, {0, 10}};
    CHECK(InsideDistance(lShape, {2.0f, 2.0f}) == doctest::Approx(2.0f));
    CHECK(InsideDistance(lShape, {7.0f, 7.0f}) < 0.0f);
    CHECK(InsideDistance(lShape, {7.0f, 2.0f}) > 0.0f);

    Shape road;
    road.kind = ShapeKind::Path;
    road.points = {{0, 50}, {100, 50}, {100, 150}};
    road.width = 4.0f;
    CHECK(InsideDistance(road, {50.0f, 51.0f}) == doctest::Approx(1.0f));
    CHECK(InsideDistance(road, {101.5f, 100.0f}) == doctest::Approx(0.5f));
    CHECK(InsideDistance(road, {50.0f, 53.0f}) < 0.0f);
}

TEST_CASE("A circle's default soft edge is the Map Editor's round brush [editor][map-script]")
{
    Shape circle;
    circle.center = {80.25f, 90.75f};
    circle.radius = 6.0f;
    const float falloff = EffectiveFalloff(circle, EdgeDefault::Soft);
    CHECK(falloff == doctest::Approx(3.0f));
    CHECK(EffectiveFalloff(circle, EdgeDefault::Hard) == 0.0f);
    const Editor::Editing::BrushCircle brush{80.25f, 90.75f, 6.0f};
    const WeightMask mask = Rasterize(circle, CellAnchor::Corner, falloff);
    for (int y = mask.rect.minY; y <= mask.rect.maxY; ++y)
        for (int x = mask.rect.minX; x <= mask.rect.maxX; ++x)
            CHECK(mask.At(x, y) ==
                  doctest::Approx(Editor::Editing::SoftWeight(brush, x, y, CellAnchor::Corner)).epsilon(1e-4));
    circle.falloff = 0.0f;
    CHECK(EffectiveFalloff(circle, EdgeDefault::Soft) == 0.0f);
}

TEST_CASE("Rectangles cover whole tiles, both corners included, clipped at the map's edges [editor][map-script]")
{
    Shape rect;
    rect.kind = ShapeKind::Rect;
    rect.tiles = CellRect{250, 0, 255, 3};
    CHECK(TileArea(rect) == 6 * 4);
    const CellRect tiles = Footprint(rect, CellAnchor::TileCentre);
    CHECK(tiles.maxX == 255);
    const WeightMask hard = Rasterize(rect, CellAnchor::TileCentre, 0.0f);
    CHECK(hard.At(250, 0) == 1.0f);
    CHECK(hard.At(255, 3) == 1.0f);
    CHECK(hard.At(249, 0) == 0.0f);
    CHECK(hard.At(250, 4) == 0.0f);
    const WeightMask corners = Rasterize(rect, CellAnchor::Corner, 0.0f);
    CHECK(corners.rect.maxX == 255); // corner 256 does not exist
    CHECK(corners.At(250, 4) == 1.0f);

    Shape circle;
    circle.center = {1.0f, 1.0f};
    circle.radius = 3.0f;
    const Bounds bounds = ShapeBounds(circle);
    CHECK(bounds.minX == 0.0f);
    CHECK(bounds.maxX == doctest::Approx(4.0f));
}

TEST_CASE("The script's random numbers are the same for a seed everywhere [editor][map-script]")
{
    Random random(0);
    CHECK(random.Next() == 0xE220A8397B1DCDAFull); // SplitMix64's published first value for seed 0
    Random a(42);
    Random b(42);
    Random c(43);
    bool differs = false;
    for (int i = 0; i < 100; ++i)
    {
        const float value = a.Uniform();
        CHECK(value == b.Uniform());
        CHECK(value >= 0.0f);
        CHECK(value < 1.0f);
        differs = differs || value != c.Uniform();
    }
    CHECK(differs);
    Random range(7);
    for (int i = 0; i < 50; ++i)
    {
        const float value = range.Range(-2.0f, 3.0f);
        CHECK(value >= -2.0f);
        CHECK(value <= 3.0f);
    }
}

TEST_CASE("Noise is smooth, stays within -1 and 1 and depends on the seed [editor][map-script]")
{
    float lowest = 1.0f;
    float highest = -1.0f;
    bool seedMatters = false;
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            const float value = FractalNoise(static_cast<float>(x), static_cast<float>(y), 8.0f, 3, 11);
            CHECK(value == FractalNoise(static_cast<float>(x), static_cast<float>(y), 8.0f, 3, 11));
            lowest = std::min(lowest, value);
            highest = std::max(highest, value);
            seedMatters =
                seedMatters || value != FractalNoise(static_cast<float>(x), static_cast<float>(y), 8.0f, 3, 12);
            const float next = FractalNoise(static_cast<float>(x) + 0.01f, static_cast<float>(y), 8.0f, 1, 11);
            const float here = FractalNoise(static_cast<float>(x), static_cast<float>(y), 8.0f, 1, 11);
            CHECK(std::fabs(next - here) < 0.05f);
        }
    }
    CHECK(lowest >= -1.0f);
    CHECK(highest <= 1.0f);
    CHECK(highest - lowest > 0.5f);
    CHECK(seedMatters);
    CHECK(FractalNoise(1.0f, 1.0f, 0.0f, 3, 1) == 0.0f);
}

TEST_CASE("Poisson scatter keeps its spacing, its count and its seed [editor][map-script]")
{
    ScatterSettings settings;
    settings.count = 40;
    settings.minSpacing = 3.0f;
    settings.bounds = Bounds{100.0f, 100.0f, 150.0f, 150.0f};
    settings.maxAttempts = settings.count * 60;
    Random first(5);
    const std::vector<Point> points = ScatterPoints(settings, first, [](Point) { return 1.0f; });
    REQUIRE(points.size() == 40);
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        CHECK(points[i].x >= 100.0f);
        CHECK(points[i].x <= 150.0f);
        for (std::size_t j = i + 1; j < points.size(); ++j)
            CHECK(std::hypot(points[i].x - points[j].x, points[i].y - points[j].y) >= 3.0f);
    }
    Random again(5);
    const std::vector<Point> repeated = ScatterPoints(settings, again, [](Point) { return 1.0f; });
    CHECK(repeated.size() == points.size());
    CHECK(repeated.front().x == points.front().x);
    CHECK(repeated.back().y == points.back().y);

    Random none(5);
    CHECK(ScatterPoints(settings, none, [](Point) { return 0.0f; }).empty());

    settings.count = 1000; // far more than 3-tile spacing fits into 50 x 50 tiles
    Random full(5);
    const std::vector<Point> crowded = ScatterPoints(settings, full, [](Point) { return 1.0f; });
    CHECK(crowded.size() < 1000);
    CHECK(crowded.size() > 150);
}

TEST_CASE("A full script parses into its ops [editor][map-script]")
{
    const json document =
        Script({{{"op", "terrain.raise"}, {"shape", Circle(100, 100, 8)}, {"amount", 60}},
                {{"op", "terrain.flatten"}, {"shape", Circle(100, 100, 3)}, {"to", {{"of", "max"}, {"plus", -5}}}},
                {{"op", "terrain.ramp"},
                 {"shape", {{"type", "rect"}, {"rect", {10, 10, 19, 12}}}},
                 {"axis", "x"},
                 {"from", "ground"},
                 {"to", 200}},
                {{"op", "texture.paint"},
                 {"shape", {{"type", "path"}, {"points", {{90, 80}, {110, 120}}}, {"width", 3}, {"falloff", 1}}},
                 {"layer", 2},
                 {"tile", "TileGround01"},
                 {"opacity", 0.8}},
                {{"op", "attribute.set"}, {"shape", Circle(20, 20, 2)}, {"value", "blocked"}},
                {{"op", "light.tint"}, {"shape", Circle(100, 100, 6)}, {"color", {1.0, 0.8, 0.6}}},
                {{"op", "object.scatter"},
                 {"models", {{{"model", "Tree01"}, {"weight", 3}}, "Shrub"}},
                 {"shape", Circle(100, 100, 20)},
                 {"count", 30},
                 {"min_spacing", 2.5},
                 {"seed", 9},
                 {"avoid",
                  {{"attributes", {"water", 4}},
                   {"slope", 30},
                   {"areas", {{{"type", "circle"}, {"center", {100, 100}}, {"radius", 3}}}}}}},
                {{"op", "object.rotate"}, {"select", {{"placed_by", 6}}}, {"by", 90}, {"pivot", "center"}}},
               "Hill");
    EditScript script;
    std::string error;
    REQUIRE_MESSAGE(ParseScript(document, script, error), error);
    REQUIRE(script.ops.size() == 8);
    CHECK(script.label == "Hill");
    CHECK(StepLabel(script) == "Hill");
    const auto& flatten = std::get<TerrainEdit>(script.ops[1].data);
    CHECK(flatten.target.reference == HeightReference::Highest);
    CHECK(flatten.target.value == -5.0f);
    const auto& ramp = std::get<TerrainEdit>(script.ops[2].data);
    CHECK(ramp.axis == RampAxis::X);
    CHECK(ramp.from.reference == HeightReference::Ground);
    CHECK(ramp.shape.tiles.maxX == 19);
    const auto& paint = std::get<TextureEdit>(script.ops[3].data);
    CHECK(paint.layer == 2);
    CHECK(paint.tile.name == "TileGround01");
    CHECK(paint.shape.falloff.value() == 1.0f);
    const auto& scatter = std::get<ScatterEdit>(script.ops[6].data);
    CHECK(scatter.models.size() == 2);
    CHECK(scatter.models[0].weight == 3.0f);
    CHECK(scatter.avoid.attributes == std::vector<std::uint16_t>{16, 4});
    CHECK(scatter.avoid.areas.size() == 1);
    const auto& rotate = std::get<ObjectEdit>(script.ops[7].data);
    CHECK(rotate.pivot == RotatePivot::Center);
    CHECK(rotate.select.placedBy.value() == 6);

    script.label.clear();
    CHECK(StepLabel(script) == "Script: terrain.raise, terrain.flatten, terrain.ramp and 5 more");
}

TEST_CASE("Script errors name the field and what it takes [editor][map-script]")
{
    CHECK(Contains(ParseError(json{{"schema", "mu-map-edit/2"}, {"ops", json::array()}}), "schema"));
    CHECK(Contains(ParseError(Script(json::array())), "ops: is a list of 1 to 256 ops"));
    CHECK(Contains(ParseError(Script({{{"op", "terrain.explode"}}})), "\"terrain.explode\" is not an op; known:"));
    CHECK(Contains(ParseError(Script({{{"op", "terrain.raise"},
                                       {"shape", {{"type", "circle"}, {"center", {1, 1}}, {"radus", 3}}},
                                       {"amount", 3}}})),
                   "ops[0].shape.radus: is not a field here; known: type, center, radius, falloff"));
    CHECK(Contains(ParseError(Script({{{"op", "terrain.raise"}, {"shape", Circle(1, 1, 500)}, {"amount", 3}}})),
                   "ops[0].shape.radius: is a number from 0.1 to 256"));
    CHECK(Contains(ParseError(Script({{{"op", "terrain.raise"}, {"shape", Circle(300, 1, 5)}, {"amount", 3}}})),
                   "ops[0].shape.center: is a point [x, y] in tiles"));
    CHECK(Contains(
        ParseError(
            Script({{{"op", "terrain.set"}, {"shape", {{"type", "rect"}, {"rect", {1.5, 1, 2, 2}}}}, {"height", 10}}})),
        "ops[0].shape.rect: is [x0, y0, x1, y1]: whole tiles"));
    CHECK(Contains(ParseError(Script({{{"op", "object.scatter"},
                                       {"model", "Tree01"},
                                       {"shape", Circle(10, 10, 5)},
                                       {"count", 3},
                                       {"density", 0.1}}})),
                   "give \"count\" (objects) or \"density\""));
    CHECK(Contains(ParseError(Script({{{"op", "object.delete"}, {"select", {{"placed_by", 3}}}}})),
                   "ops[0].select.placed_by: names an earlier object.place or object.scatter op"));
    CHECK(Contains(ParseError(Script({{{"op", "object.delete"}, {"select", json::object()}}})),
                   "ops[0].select: needs at least one of"));
    CHECK(Contains(ParseError(Script({{{"op", "attribute.set"}, {"shape", Circle(5, 5, 2)}, {"value", 5}}})),
                   "clean single values only"));
    CHECK(Contains(
        ParseError(Script({{{"op", "texture.paint"}, {"shape", Circle(5, 5, 2)}, {"tile", 2}, {"opacity", 0.5}}})),
        "ops[0].layer:"));
    CHECK(Contains(ParseError(Script({{{"op", "terrain.ramp"}, {"shape", Circle(5, 5, 2)}, {"from", 1}, {"to", 2}}})),
                   "a ramp runs along a \"path\" or across a \"rect\""));
    CHECK(Contains(ParseError(Script({{{"op", "object.place"}, {"model", "Tree01"}, {"tile", {256, 3}}}})),
                   "x and y below 256"));
    EditScript script;
    std::string error;
    CHECK_FALSE(ParseScript(std::string("{not json"), script, error));
    CHECK(error == "the script is not valid JSON");
}

TEST_CASE("Names resolve to the models and textures the map has loaded [editor][map-script]")
{
    const MapContext context = TestContext();
    EditScript script;
    std::string error;
    REQUIRE(ParseScript(Script({{{"op", "object.place"}, {"model", "shrub"}, {"tile", {5, 5}}},
                                {{"op", "object.place"}, {"model", "house01"}, {"tile", {5, 5}}},
                                {{"op", "object.place"}, {"model", 5}, {"tile", {5, 5}}},
                                {{"op", "texture.paint"}, {"shape", Circle(5, 5, 2)}, {"tile", "tilewater01"}}}),
                        script, error));
    REQUIRE_MESSAGE(ResolveNames(script, context, error), error);
    CHECK(std::get<PlaceEdit>(script.ops[0].data).model.type == BUSH);
    CHECK(std::get<PlaceEdit>(script.ops[1].data).model.type == HOUSE);
    CHECK(std::get<TextureEdit>(script.ops[3].data).tile.slot == 5);
    CHECK(ModelDisplayName(context, HOUSE) == "House01");
    CHECK(ModelDisplayName(context, 99) == "type 99");

    REQUIRE(ParseScript(Script({{{"op", "object.place"}, {"model", "Tree99"}, {"tile", {5, 5}}}}), script, error));
    CHECK_FALSE(ResolveNames(script, context, error));
    CHECK(Contains(error, "ops[0].model: no model called \"Tree99\" is loaded on this map; loaded: Tree01 (type " +
                              std::to_string(TREE) + "), Shrub (type " + std::to_string(BUSH) + ")"));
    REQUIRE(ParseScript(Script({{{"op", "texture.paint"}, {"shape", Circle(5, 5, 2)}, {"tile", 3}}}), script, error));
    CHECK_FALSE(ResolveNames(script, context, error));
    CHECK(Contains(error, "ops[0].tile: texture slot 3 holds no texture on this map"));
}

TEST_CASE("Raise, lower, set and flatten shape the ground and clamp it [editor][map-script]")
{
    MapState state = FlatMap();
    REQUIRE(Run(Script({{{"op", "terrain.raise"}, {"shape", Circle(100, 100, 10)}, {"amount", 50}}}), state).empty());
    CHECK(HeightAt(state, 100, 100) == doctest::Approx(150.0f));
    CHECK(HeightAt(state, 104, 100) == doctest::Approx(150.0f)); // inside the core (half the radius)
    CHECK(HeightAt(state, 108, 100) < 150.0f);
    CHECK(HeightAt(state, 108, 100) > 100.0f);
    CHECK(HeightAt(state, 110, 100) == doctest::Approx(100.0f));

    REQUIRE(Run(Script({{{"op", "terrain.lower"}, {"shape", Circle(20, 20, 4)}, {"amount", 500}}}), state).empty());
    CHECK(HeightAt(state, 20, 20) == 0.0f); // clamped at 0

    REQUIRE(Run(Script({{{"op", "terrain.set"},
                         {"shape", {{"type", "rect"}, {"rect", {30, 30, 39, 39}}, {"falloff", 0}}},
                         {"height", 300}}}),
                state)
                .empty());
    CHECK(HeightAt(state, 30, 30) == 300.0f);
    CHECK(HeightAt(state, 40, 40) == 300.0f); // the far corners of the last tiles
    CHECK(HeightAt(state, 41, 40) == GROUND);

    REQUIRE(Run(Script({{{"op", "terrain.flatten"}, {"shape", Circle(100, 100, 10)}, {"to", "min"}}}), state).empty());
    CHECK(HeightAt(state, 100, 100) == doctest::Approx(GROUND).epsilon(0.01)); // the rim barely rose

    std::string error = Run(Script({{{"op", "terrain.set"}, {"shape", Circle(60, 60, 4)}, {"height", 500}}}), state);
    CHECK(Contains(error, "ops[0].height: the height comes to 500"));
}

TEST_CASE("A ramp runs from one height to the other along a path or across a rectangle [editor][map-script]")
{
    MapState state = FlatMap();
    REQUIRE(
        Run(Script({{{"op", "terrain.ramp"},
                     {"shape", {{"type", "path"}, {"points", {{100, 100}, {140, 100}}}, {"width", 6}, {"falloff", 0}}},
                     {"from", 100},
                     {"to", 300}}}),
            state)
            .empty());
    CHECK(HeightAt(state, 100, 100) == doctest::Approx(100.0f));
    CHECK(HeightAt(state, 120, 100) == doctest::Approx(200.0f));
    CHECK(HeightAt(state, 140, 101) == doctest::Approx(300.0f));

    REQUIRE(Run(Script({{{"op", "terrain.ramp"},
                         {"shape", {{"type", "rect"}, {"rect", {10, 10, 19, 14}}, {"falloff", 0}}},
                         {"axis", "y"},
                         {"from", "ground"},
                         {"to", {{"of", "ground"}, {"plus", 50}}}}}),
                state)
                .empty());
    CHECK(HeightAt(state, 12, 10) == doctest::Approx(GROUND));
    CHECK(HeightAt(state, 12, 15) == doctest::Approx(GROUND + 50.0f));
    CHECK(HeightAt(state, 12, 12) == doctest::Approx(GROUND + 20.0f));
}

TEST_CASE("Smooth evens out and noise is deterministic [editor][map-script]")
{
    MapState state = FlatMap();
    for (int y = 90; y <= 110; ++y)
        for (int x = 90; x <= 110; ++x)
            state.terrain.height[Editor::MapInspect::CellIndex(x, y)] = (x + y) % 2 == 0 ? 50.0f : 150.0f;
    REQUIRE(
        Run(Script({{{"op", "terrain.smooth"}, {"shape", Circle(100, 100, 8)}, {"iterations", 5}}}), state).empty());
    // A checkerboard is the five-point kernel's worst case: each pass keeps -0.6 of the
    // step between neighbours (a cell of +50 averages to (50 - 4 x 50) / 5 = -30), so five
    // passes leave 100 x 0.6^5 = 7.8 of the 100.
    const float step = std::fabs(HeightAt(state, 100, 100) - HeightAt(state, 101, 100));
    CHECK(step < 10.0f);
    CHECK(step > 5.0f);

    MapState first = FlatMap();
    MapState second = FlatMap();
    const json noise =
        Script({{{"op", "terrain.noise"}, {"shape", Circle(100, 100, 20)}, {"amplitude", 30}, {"seed", 4}}});
    REQUIRE(Run(noise, first).empty());
    REQUIRE(Run(noise, second).empty());
    CHECK(first.terrain.height == second.terrain.height);
    CHECK(HeightAt(first, 100, 100) != GROUND);
    CHECK(std::fabs(HeightAt(first, 100, 100) - GROUND) <= 30.0f);
}

TEST_CASE("Objects follow the ground they stand on unless told not to [editor][map-script]")
{
    MapState state = FlatMap(); // objects 0 and 1 stand at tiles (50, 50) and (60, 50)
    REQUIRE(
        Run(Script({{{"op", "terrain.raise"}, {"shape", Circle(50.5f, 50.5f, 4)}, {"amount", 40}}}), state).empty());
    CHECK(state.objects[0].state.position[2] == doctest::Approx(GROUND + 40.0f));
    CHECK(state.objects[1].state.position[2] == GROUND);
    REQUIRE(Run(Script({{{"op", "terrain.raise"},
                         {"shape", Circle(60.5f, 50.5f, 4)},
                         {"amount", 40},
                         {"objects_follow", false}}}),
                state)
                .empty());
    CHECK(state.objects[1].state.position[2] == GROUND);
}

TEST_CASE("Texture paint sets layer 1 tiles along a road and blends layer 2 softly [editor][map-script]")
{
    MapState state = FlatMap();
    REQUIRE(Run(Script({{{"op", "texture.paint"},
                         {"shape", {{"type", "path"}, {"points", {{10, 20.5}, {40, 20.5}}}, {"width", 3}}},
                         {"tile", "TileGround01"}}}),
                state)
                .empty());
    CHECK(state.terrain.baseTiles[Editor::MapInspect::CellIndex(20, 20)] == DIRT_SLOT);
    CHECK(state.terrain.baseTiles[Editor::MapInspect::CellIndex(20, 21)] == DIRT_SLOT);
    CHECK(state.terrain.baseTiles[Editor::MapInspect::CellIndex(20, 22)] == GRASS_SLOT);
    CHECK(state.terrain.baseTiles[Editor::MapInspect::CellIndex(45, 20)] == GRASS_SLOT);

    REQUIRE(Run(Script({{{"op", "texture.paint"},
                         {"shape", Circle(80, 80, 6)},
                         {"layer", 2},
                         {"tile", 2},
                         {"opacity", 0.75}}}),
                state)
                .empty());
    const std::size_t core = Editor::MapInspect::CellIndex(80, 80);
    CHECK(state.terrain.overlayTiles[core] == DIRT_SLOT);
    CHECK(state.terrain.overlayAlpha[core] == doctest::Approx(0.75f));
    CHECK(state.terrain.overlayAlpha[Editor::MapInspect::CellIndex(85, 80)] < 0.75f);
    REQUIRE(Run(Script({{{"op", "texture.erase"}, {"shape", Circle(80, 80, 6)}}}), state).empty());
    CHECK(state.terrain.overlayTiles[core] == 255);
}

TEST_CASE("Walkability takes clean values and never the anti-tamper tile [editor][map-script]")
{
    MapState state = FlatMap();
    REQUIRE(
        Run(Script(
                {{{"op", "attribute.set"}, {"shape", {{"type", "rect"}, {"rect", {5, 5, 6, 6}}}}, {"value", "water"}}}),
            state)
            .empty());
    CHECK(AttributeAt(state, 5, 5) == Attributes::WATER);
    CHECK(AttributeAt(state, 6, 6) == Attributes::WATER);
    CHECK(AttributeAt(state, 7, 6) == 0);

    MapState lorencia = FlatMap();
    lorencia.terrain.attribute[Editor::MapInspect::CellIndex(135, 123)] = 5;
    std::vector<OpReport> reports;
    const std::string error = Run(Script({{{"op", "attribute.set"}, {"shape", Circle(135, 123, 3)}, {"value", 0}}}),
                                  lorencia, reports, TestContext(LORENCIA));
    CHECK(Contains(error, "would set tile (135, 123) to walkable"));
    CHECK(Contains(error, "gotcha 13"));
    // Other maps have their own tile (or none), and Dungeon's holds 4.
    CHECK(Run(Script({{{"op", "attribute.set"}, {"shape", Circle(135, 123, 3)}, {"value", 0}}}), lorencia).empty());
    MapState dungeon = FlatMap();
    CHECK(Run(Script({{{"op", "attribute.set"}, {"shape", Circle(227.5f, 120.5f, 1)}, {"value", "blocked"}}}), dungeon,
              reports, TestContext(1))
              .empty());
}

TEST_CASE("Light ops add, subtract, tint, set and smooth within 0 and 1 [editor][map-script]")
{
    MapState state = FlatMap();
    auto light = [&state](int x, int y, int c)
    { return state.terrain.light[Editor::MapInspect::CellIndex(x, y) * 3 + c]; };
    REQUIRE(Run(Script({{{"op", "light.add"}, {"shape", Circle(50, 50, 6)}, {"color", {1, 0, 0}}, {"strength", 0.8}}}),
                state)
                .empty());
    CHECK(light(50, 50, 0) == doctest::Approx(1.0f)); // clamped
    CHECK(light(50, 50, 1) == doctest::Approx(0.5f));
    REQUIRE(Run(Script({{{"op", "light.subtract"}, {"shape", Circle(80, 80, 6)}, {"strength", 0.3}}}), state).empty());
    CHECK(light(80, 80, 2) == doctest::Approx(0.2f));
    REQUIRE(
        Run(Script({{{"op", "light.set"}, {"shape", Circle(120, 80, 6)}, {"color", {0.1, 0.2, 0.3}}}}), state).empty());
    CHECK(light(120, 80, 1) == doctest::Approx(0.2f));
    REQUIRE(Run(Script({{{"op", "light.tint"}, {"shape", Circle(160, 80, 6)}, {"color", {1, 1, 1}}}}), state).empty());
    CHECK(light(160, 80, 0) == doctest::Approx(0.75f));
    REQUIRE(Run(Script({{{"op", "light.smooth"}, {"shape", Circle(120, 80, 8)}, {"iterations", 3}}}), state).empty());
    CHECK(light(123, 80, 1) > 0.21f); // the set patch's edge takes some of the brighter light around it
}

TEST_CASE("Light bake shades slopes from the sun and leaves flat ground as it was [editor][map-script]")
{
    MapState state = FlatMap();
    auto light = [&state](int x, int y) { return state.terrain.light[Editor::MapInspect::CellIndex(x, y) * 3]; };
    // A ridge along y = 100, rising towards y (north): its south face looks south, its north face north.
    for (int y = 90; y <= 110; ++y)
    {
        for (int x = 80; x <= 120; ++x)
            state.terrain.height[Editor::MapInspect::CellIndex(x, y)] = GROUND + 20.0f * (10 - std::abs(y - 100));
    }
    const json rect = {{"type", "rect"}, {"rect", {0, 0, 255, 255}}, {"falloff", 0}};
    std::vector<OpReport> reports;
    REQUIRE(Run(Script({{{"op", "light.bake"}, {"shape", rect}, {"azimuth", 180}, {"elevation", 35}}}), state, reports)
                .empty());
    CHECK(light(40, 40) == doctest::Approx(0.5f)); // flat: unchanged
    CHECK(light(100, 95) > 0.55f);                 // faces the southern sun: brighter
    CHECK(light(100, 105) < 0.45f);                // turns away: darker
    CHECK(light(100, 95) <= 1.0f);
    CHECK(reports[0].area.minX == 0);

    // contrast 0 changes nothing; a sun from the north turns it round.
    MapState copy = state;
    REQUIRE(Run(Script({{{"op", "light.bake"}, {"shape", rect}, {"contrast", 0}}}), copy).empty());
    CHECK(copy.terrain.light == state.terrain.light);
    CHECK(Contains(ParseError(Script({{{"op", "light.bake"}, {"shape", rect}, {"elevation", 2}}})), "elevation"));
    CHECK(Contains(ParseError(Script({{{"op", "light.bake"}, {"shape", rect}, {"color", {1, 1, 1}}}})),
                   "color: is not a field here"));
}

TEST_CASE("Walkability goes under the objects a selector picks [editor][map-script]")
{
    MapState state = FlatMap();
    std::vector<OpReport> reports;
    REQUIRE(
        Run(Script({{{"op", "attribute.set"}, {"under", {{"model", "Tree01"}}}, {"value", "blocked"}}}), state, reports)
            .empty());
    CHECK(AttributeAt(state, 50, 50) == Attributes::BLOCKED);
    CHECK(AttributeAt(state, 60, 50) == Attributes::BLOCKED);
    CHECK(AttributeAt(state, 120, 120) == 0); // the house is not a tree
    CHECK(reports[0].selected == 2);

    // Objects a scatter placed earlier in the same script.
    MapState forest = FlatMap();
    REQUIRE(Run(Script({{{"op", "object.scatter"}, {"model", "Shrub"}, {"shape", Circle(150, 150, 10)}, {"count", 4}},
                        {{"op", "attribute.set"}, {"under", {{"placed_by", 0}}}, {"value", 4}}}),
                forest)
                .empty());
    for (const MapObject* shrub : PlacedBy(forest, 0))
    {
        const int x = static_cast<int>(std::floor(shrub->state.position[0] / TILE_WORLD));
        const int y = static_cast<int>(std::floor(shrub->state.position[1] / TILE_WORLD));
        CHECK(AttributeAt(forest, x, y) == Attributes::BLOCKED);
    }

    CHECK(Contains(Run(Script({{{"op", "attribute.set"}, {"under", {{"model", "Shrub"}}}, {"value", 4}}}), state),
                   "ops[0].under: matches no object"));
    CHECK(Contains(ParseError(Script({{{"op", "attribute.set"}, {"value", 4}}})), "\"shape\""));
    CHECK(Contains(
        ParseError(Script(
            {{{"op", "attribute.set"}, {"shape", Circle(5, 5, 2)}, {"under", {{"model", "Tree01"}}}, {"value", 4}}})),
        "one of them"));
    CHECK(Contains(ParseError(Script({{{"op", "attribute.set"}, {"under", {{"placed_by", 0}}}, {"value", 4}}})),
                   "ops[0].under.placed_by"));
}

TEST_CASE("A script that would hold the client for seconds is refused before it runs [editor][map-script]")
{
    // 256 noise ops, each over a long wide path of 256 points: about ten seconds of main loop.
    json points = json::array();
    for (int i = 0; i < 256; ++i)
        points.push_back({static_cast<float>(i), 128.0f + static_cast<float>(i % 7)});
    const json path = {{"type", "path"}, {"points", points}, {"width", 64}};
    json ops = json::array();
    for (int i = 0; i < 256; ++i)
        ops.push_back({{"op", "terrain.noise"}, {"shape", path}, {"amplitude", 10}, {"octaves", 8}});
    EditScript heavy;
    std::string error;
    REQUIRE(ParseScript(Script(ops), heavy, error));
    CHECK_FALSE(CheckScriptCost(heavy, error));
    CHECK(Contains(error, "Split it into several scripts"));
    CHECK(Contains(error, "ops[0] (terrain.noise) alone takes about"));

    MapState state = FlatMap();
    CHECK(Contains(Run(Script(ops), state), "would hold the client"));
    CHECK(state.terrain.height == FlatMap().terrain.height);

    // An ordinary scene: a hill, a road, a grove, light over the whole map.
    EditScript ordinary;
    REQUIRE(ParseScript(Script({{{"op", "terrain.raise"}, {"shape", Circle(100, 100, 20)}, {"amount", 60}},
                                {{"op", "terrain.smooth"}, {"shape", Circle(100, 100, 30)}, {"iterations", 8}},
                                {{"op", "light.bake"}, {"shape", {{"type", "rect"}, {"rect", {0, 0, 255, 255}}}}},
                                {{"op", "texture.paint"}, {"layer", 2}, {"tile", 2}, {"shape", path}}}),
                        ordinary, error));
    CHECK(CheckScriptCost(ordinary, error));
    CHECK(OpCost(ordinary.ops[0]) < OpCost(ordinary.ops[3]));
}

TEST_CASE("Place puts one object on the ground or above it [editor][map-script]")
{
    MapState state = FlatMap();
    std::vector<OpReport> reports;
    REQUIRE(
        Run(Script(
                {{{"op", "object.place"}, {"model", "Tree01"}, {"tile", {10.5, 20.5}}, {"angle", 90}, {"scale", 1.5}},
                 {{"op", "object.place"},
                  {"model", "house01"},
                  {"tile", {30, 30}},
                  {"height", {{"offset", 25}}},
                  {"angle", {0, 0, 45}}}}),
            state, reports)
            .empty());
    REQUIRE(state.objects.size() == 5);
    const MapObject& tree = state.objects[3];
    CHECK(tree.key == NEW_OBJECT);
    CHECK(tree.id == 3);
    CHECK(tree.state.position[0] == doctest::Approx(1050.0f));
    CHECK(tree.state.position[2] == doctest::Approx(GROUND));
    CHECK(tree.state.angle[2] == 90.0f);
    CHECK(tree.state.scale == 1.5f);
    CHECK(state.objects[4].state.position[2] == doctest::Approx(GROUND + 25.0f));
    CHECK(reports[1].ids == std::vector<int>{4});
}

TEST_CASE("Scatter places a seeded Poisson-disk forest that keeps off the road [editor][map-script]")
{
    const json road = {{"type", "path"}, {"points", {{80, 100}, {120, 100}}}, {"width", 4}};
    const json script = Script({{{"op", "object.scatter"},
                                 {"models", {"Tree01", {{"model", "Shrub"}, {"weight", 0.5}}}},
                                 {"shape", Circle(100, 100, 18)},
                                 {"count", 30},
                                 {"min_spacing", 3},
                                 {"seed", 21},
                                 {"scale_range", {0.8, 1.2}},
                                 {"yaw_range", {0, 360}},
                                 {"avoid", {{"areas", {road}}}},
                                 {"mark_attribute", "blocked"}}});
    MapState state = FlatMap();
    std::vector<OpReport> reports;
    REQUIRE(Run(script, state, reports).empty());
    const std::vector<const MapObject*> trees = PlacedBy(state, 0);
    REQUIRE(trees.size() == 30);
    CHECK(reports[0].placed == 30);
    CHECK(reports[0].requested == 30);
    Shape roadShape;
    roadShape.kind = ShapeKind::Path;
    roadShape.points = {{80, 100}, {120, 100}};
    roadShape.width = 4.0f;
    for (const MapObject* tree : trees)
    {
        const Point at{tree->state.position[0] / TILE_WORLD, tree->state.position[1] / TILE_WORLD};
        CHECK(InsideDistance(roadShape, at) < 0.0f);
        CHECK(std::hypot(at.x - 100.0f, at.y - 100.0f) <= 18.0f);
        CHECK(tree->state.scale >= 0.8f);
        CHECK(tree->state.scale <= 1.2f);
        CHECK((tree->state.type == TREE || tree->state.type == BUSH));
        CHECK(tree->state.position[2] == doctest::Approx(GROUND));
        CHECK(AttributeAt(state, static_cast<int>(at.x), static_cast<int>(at.y)) == Attributes::BLOCKED);
        for (const MapObject* other : trees)
        {
            if (other != tree)
                CHECK(std::hypot(tree->state.position[0] - other->state.position[0],
                                 tree->state.position[1] - other->state.position[1]) >= 300.0f);
        }
    }

    MapState again = FlatMap();
    REQUIRE(Run(script, again).empty());
    REQUIRE(again.objects.size() == state.objects.size());
    for (std::size_t i = 0; i < state.objects.size(); ++i)
        CHECK(again.objects[i].state == state.objects[i].state);
}

TEST_CASE(
    "Scatter avoids walkability, textures, slopes and objects, and says when it ran out of room [editor][map-script]")
{
    MapState state = FlatMap();
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 128; ++x)
            state.terrain.attribute[Editor::MapInspect::CellIndex(x, y)] = Attributes::WATER;
    std::vector<OpReport> reports;
    REQUIRE(Run(Script({{{"op", "object.scatter"},
                         {"model", "Tree01"},
                         {"shape", {{"type", "rect"}, {"rect", {100, 100, 155, 155}}}},
                         {"density", 0.01},
                         {"avoid", {{"attributes", {"water"}}, {"objects", 5}}}}}),
                state, reports)
                .empty());
    CHECK(reports[0].requested == 31); // 56 x 56 tiles at 0.01 per tile
    for (const MapObject* tree : PlacedBy(state, 0))
    {
        CHECK(tree->state.position[0] >= 128.0f * TILE_WORLD);
        CHECK(std::hypot(tree->state.position[0] - 12050.0f, tree->state.position[1] - 12050.0f) >= 500.0f);
    }

    MapState tight = FlatMap();
    REQUIRE(Run(Script({{{"op", "object.scatter"},
                         {"model", "Tree01"},
                         {"shape", Circle(200, 200, 3)},
                         {"count", 50},
                         {"min_spacing", 2}}}),
                tight, reports)
                .empty());
    CHECK(reports[0].placed < 50);
    REQUIRE(reports[0].warnings.size() == 1);
    CHECK(Contains(reports[0].warnings[0], "of 50: the shape has no more room"));
}

TEST_CASE("Object edits move, turn, scale, drop and delete the selected objects [editor][map-script]")
{
    MapState state = FlatMap();
    state.terrain.height.assign(state.terrain.height.size(), 200.0f);
    REQUIRE(Run(Script({{{"op", "object.move"}, {"select", {{"ids", {0}}}}, {"by", {2, -1}}},
                        {{"op", "object.rotate"}, {"select", {{"model", "Tree01"}}}, {"by", 30}},
                        {{"op", "object.scale"}, {"select", {{"model", "House01"}}}, {"to", 2}},
                        {{"op", "object.drop_to_ground"}, {"select", {{"ids", {1}}}}}}),
                state)
                .empty());
    CHECK(state.objects[0].state.position[0] == doctest::Approx(52.5f * TILE_WORLD));
    CHECK(state.objects[0].state.position[1] == doctest::Approx(49.5f * TILE_WORLD));
    // The ground is at 200 and the tree stood 100 below it; a move keeps that.
    CHECK(state.objects[0].state.position[2] == doctest::Approx(GROUND));
    CHECK(state.objects[0].state.angle[2] == doctest::Approx(30.0f));
    CHECK(state.objects[1].state.angle[2] == doctest::Approx(30.0f));
    CHECK(state.objects[2].state.scale == 2.0f);
    CHECK(state.objects[1].state.position[2] == doctest::Approx(200.0f));

    REQUIRE(Run(Script({{{"op", "object.rotate"},
                         {"select", {{"inside", {{"type", "rect"}, {"rect", {40, 40, 70, 60}}}}}},
                         {"by", 180},
                         {"pivot", "center"}}}),
                state)
                .empty());
    // The two trees swap sides around their middle.
    CHECK(state.objects[0].state.position[0] > state.objects[1].state.position[0]);

    REQUIRE(Run(Script({{{"op", "object.delete"}, {"select", {{"model", "Tree01"}}}}}), state).empty());
    REQUIRE(state.objects.size() == 1);
    CHECK(state.objects[0].id == 2);

    CHECK(Contains(Run(Script({{{"op", "object.delete"}, {"select", {{"ids", {0}}}}}}), state),
                   "ops[0].select.ids: no object has id 0"));
    CHECK(Contains(Run(Script({{{"op", "object.move"}, {"select", {{"model", "Tree01"}}}, {"by", {1, 1}}}}), state),
                   "ops[0].select: matches no object"));
    CHECK(Contains(Run(Script({{{"op", "object.move"}, {"select", {{"ids", {2}}}}, {"by", {200, 0}}}}), state),
                   "would leave the map"));
}

TEST_CASE("Later ops select what earlier ones placed, and the object file's limit holds [editor][map-script]")
{
    MapState state = FlatMap();
    std::vector<OpReport> reports;
    REQUIRE(Run(Script({{{"op", "object.scatter"}, {"model", "Shrub"}, {"shape", Circle(150, 150, 10)}, {"count", 5}},
                        {{"op", "object.scale"}, {"select", {{"placed_by", 0}}}, {"by", 2}}}),
                state, reports)
                .empty());
    CHECK(reports[1].selected == 5);
    for (const MapObject* shrub : PlacedBy(state, 0))
        CHECK(shrub->state.scale == 2.0f);

    MapContext limited = TestContext();
    limited.maxObjects = 4;
    MapState full = FlatMap();
    const std::string error =
        Run(Script({{{"op", "object.scatter"}, {"model", "Shrub"}, {"shape", Circle(150, 150, 10)}, {"count", 5}}}),
            full, reports, limited);
    CHECK(Contains(error, "ops[0] (object.scatter): the map would hold 8 objects; its object file holds at most 4"));
}

TEST_CASE("A failing op names itself and leaves a half-run copy the caller drops [editor][map-script]")
{
    const MapState original = FlatMap();
    MapState working = original;
    std::vector<OpReport> reports;
    const std::string error = Run(Script({{{"op", "terrain.raise"}, {"shape", Circle(100, 100, 10)}, {"amount", 50}},
                                          {{"op", "object.place"}, {"model", "Rock99"}, {"tile", {5, 5}}}}),
                                  working, reports);
    CHECK(Contains(error, "ops[1].model: no model called \"Rock99\""));
    // Names are checked before any op runs, so even the copy is untouched.
    CHECK(working.terrain.height == original.terrain.height);
    const MapChanges nothing = Diff(original, working);
    CHECK_FALSE(nothing.Any());
}

TEST_CASE("The diff counts the cells and objects a script changed [editor][map-script]")
{
    const MapState before = FlatMap();
    MapState after = before;
    REQUIRE(
        Run(Script({{{"op", "terrain.set"},
                     {"shape", {{"type", "rect"}, {"rect", {10, 10, 11, 11}}, {"falloff", 0}}},
                     {"height", 150}},
                    {{"op", "attribute.set"}, {"shape", {{"type", "rect"}, {"rect", {20, 20, 22, 20}}}}, {"value", 4}},
                    {{"op", "object.place"}, {"model", "Tree01"}, {"tile", {70, 70}}},
                    {{"op", "object.delete"}, {"select", {{"ids", {2}}}}},
                    {{"op", "object.move"}, {"select", {{"ids", {0}}}}, {"by", {1, 0}}}}),
            after)
            .empty());
    const MapChanges changes = Diff(before, after);
    CHECK(changes.height.cells == 9); // corners 10..12 x 10..12
    CHECK(changes.height.area.minX == 10);
    CHECK(changes.height.area.maxY == 12);
    CHECK(changes.attribute.cells == 3);
    CHECK_FALSE(changes.light.Changed());
    CHECK(changes.Count(ObjectChangeKind::Added) == 1);
    CHECK(changes.Count(ObjectChangeKind::Removed) == 1);
    CHECK(changes.Count(ObjectChangeKind::Changed) == 1);
    CHECK(changes.objects.front().kind == ObjectChangeKind::Removed);
    CHECK(changes.objects.front().key == 1002);

    const json report = ReportJson({}, changes, TestContext());
    CHECK(report["changes"]["height"]["cells"] == 9);
    CHECK(report["changes"]["height"]["area"] == json::array({10, 10, 12, 12}));
    CHECK(report["changes"]["texture1"]["area"].is_null());
    CHECK(report["changes"]["objects"]["added"] == 1);
    CHECK(report["changes"]["objects"]["list"][0]["change"] == "removed");
    CHECK(report["changes"]["objects"]["list"][0]["model"] == "House01");
    CHECK(report["changes"]["objects"]["list"][1]["before"]["tile"] == json::array({50, 50}));
}

TEST_CASE("A client walkability file is read only when the client would load it [editor][map-script]")
{
    std::vector<std::uint8_t> plain(4 + 65536, 0);
    plain[1] = 1;
    plain[2] = 255;
    plain[3] = 255;
    plain[4 + 123 * 256 + 135] = 5;
    std::vector<std::uint16_t> tiles;
    std::string error;
    CHECK(Attributes::DecodeClientFile(plain.data(), plain.size(), LORENCIA, tiles, error));
    CHECK(tiles[123 * 256 + 135] == 5);

    plain[4 + 123 * 256 + 135] = 4;
    CHECK_FALSE(Attributes::DecodeClientFile(plain.data(), plain.size(), LORENCIA, tiles, error));
    CHECK(Contains(error, "tile (135, 123) holds 4"));
    CHECK(Attributes::DecodeClientFile(plain.data(), plain.size(), OTHER_MAP, tiles, error));

    plain[10] = 200;
    CHECK_FALSE(Attributes::DecodeClientFile(plain.data(), plain.size(), OTHER_MAP, tiles, error));
    CHECK(Contains(error, "128 or more"));
    CHECK_FALSE(Attributes::DecodeClientFile(plain.data(), 100, OTHER_MAP, tiles, error));
    plain[0] = 1;
    CHECK_FALSE(Attributes::DecodeClientFile(plain.data(), plain.size(), OTHER_MAP, tiles, error));

    std::uint16_t value = 0;
    CHECK(Attributes::FromName("Blocked", value));
    CHECK(value == 4);
    CHECK(Attributes::Matches(5, Attributes::BLOCKED));
    CHECK_FALSE(Attributes::Matches(1, Attributes::WALKABLE));
    CHECK(Attributes::Matches(0, Attributes::WALKABLE));
}

TEST_CASE("Ground height and slope read the terrain as the engine does [editor][map-script]")
{
    MapTerrain terrain = MapTerrain::Filled(0, 0.0f, 0.5f);
    terrain.height[Editor::MapInspect::CellIndex(11, 10)] = 100.0f;
    CHECK(GroundHeight(terrain, 1000.0f, 1000.0f, 1200.0f) == 0.0f);
    CHECK(GroundHeight(terrain, 1100.0f, 1000.0f, 1200.0f) == doctest::Approx(100.0f));
    CHECK(GroundHeight(terrain, 1050.0f, 1000.0f, 1200.0f) == doctest::Approx(50.0f));
    CHECK(GroundHeight(terrain, -5.0f, 1000.0f, 1200.0f) == 0.0f);
    terrain.attribute[Editor::MapInspect::CellIndex(10, 10)] = 0x40; // special height
    CHECK(GroundHeight(terrain, 1050.0f, 1050.0f, 1200.0f) == 1200.0f);
    // Rises of 0.5 along x and -0.5 along y: atan(sqrt(0.5)) = 35.26 degrees.
    CHECK(SlopeDegrees(terrain, Point{10.5f, 10.5f}) == doctest::Approx(35.264f).epsilon(0.001));
    CHECK(SlopeDegrees(terrain, Point{50.5f, 50.5f}) == 0.0f);
}
