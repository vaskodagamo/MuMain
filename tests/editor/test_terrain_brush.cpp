#include <doctest.h>

#include "Editing/EditCommandGroup.h"
#include "Editing/FieldBrush.h"
#include "Editing/SurfaceBrush.h"
#include "Editing/TerrainBrush.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Editor::Editing;

namespace
{
constexpr int MAP_SIZE = 256;
constexpr int LAST = MAP_SIZE - 1;

std::size_t Index(int x, int y)
{
    return static_cast<std::size_t>(y) * MAP_SIZE + static_cast<std::size_t>(x);
}

// A height map (or a light map with `channels` 3) filled with `value`.
struct TestField
{
    explicit TestField(float value, int channels = 1)
        : values(static_cast<std::size_t>(MAP_SIZE) * MAP_SIZE * channels, value)
    {
        field = {values.data(), MAP_SIZE, MAP_SIZE, channels};
    }
    float& At(int x, int y, int c = 0)
    {
        return values[Index(x, y) * static_cast<std::size_t>(field.channels) + static_cast<std::size_t>(c)];
    }

    std::vector<float> values;
    FloatField field;
};

struct TestOverlay
{
    TestOverlay() : tiles(MAP_SIZE * MAP_SIZE, NO_OVERLAY_TILE), alpha(MAP_SIZE * MAP_SIZE, 0.0f)
    {
        layer = {tiles.data(), alpha.data(), MAP_SIZE, MAP_SIZE};
    }

    std::vector<std::uint8_t> tiles;
    std::vector<float> alpha;
    OverlayLayer layer;
};

// A command that records how it was applied.
class LoggedCommand : public EditCommand
{
public:
    LoggedCommand(std::string label, std::vector<std::string>& log) : EditCommand(std::move(label)), m_log(log) {}
    bool Undo() override
    {
        m_log.push_back("undo " + Label());
        return true;
    }
    bool Redo() override
    {
        m_log.push_back("redo " + Label());
        return true;
    }
    std::size_t MemoryBytes() const override
    {
        return 10;
    }

private:
    std::vector<std::string>& m_log;
};
} // namespace

TEST_CASE("The soft brush is full out to half its radius and fades smoothly to the rim [editor][brush]")
{
    constexpr float RADIUS = 4.0f;
    CHECK(Falloff(0.0f, RADIUS) == 1.0f);
    CHECK(Falloff(RADIUS * FALLOFF_START, RADIUS) == 1.0f);
    CHECK(Falloff(RADIUS, RADIUS) == 0.0f);
    CHECK(Falloff(RADIUS * 2.0f, RADIUS) == 0.0f);
    const float middleOfFade = RADIUS * (FALLOFF_START + 1.0f) * 0.5f;
    CHECK(Falloff(middleOfFade, RADIUS) == doctest::Approx(0.5f));

    float previous = 1.0f;
    for (int step = 1; step <= 100; ++step)
    {
        const float weight = Falloff(RADIUS * static_cast<float>(step) / 100.0f, RADIUS);
        CHECK(weight <= previous);
        previous = weight;
    }
    CHECK(Falloff(1.0f, 0.0f) == 0.0f);
}

TEST_CASE("A brush's footprint is clipped at the map's edges [editor][brush]")
{
    const CellRect corner = Footprint({0.3f, 0.3f, 3.0f}, MAP_SIZE, MAP_SIZE);
    CHECK(corner.minX == 0);
    CHECK(corner.minY == 0);
    CHECK(corner.maxX == 4);
    CHECK(corner.maxY == 4);

    const CellRect farEdge = Footprint({254.5f, 10.0f, 4.0f}, MAP_SIZE, MAP_SIZE);
    CHECK(farEdge.maxX == LAST);
    CHECK(farEdge.minX == 250);

    CHECK(Footprint({-10.0f, -10.0f, 2.0f}, MAP_SIZE, MAP_SIZE).IsEmpty());
    CHECK(Footprint({10.0f, 10.0f, 0.0f}, MAP_SIZE, MAP_SIZE).IsEmpty());

    const CellRect grown = Grow({0, 250, 3, LAST}, 1, MAP_SIZE, MAP_SIZE);
    CHECK(grown.minX == 0);
    CHECK(grown.minY == 249);
    CHECK(grown.maxX == 4);
    CHECK(grown.maxY == LAST);
}

TEST_CASE("Raising at a map corner never reaches the far side of the map [editor][brush]")
{
    TestField heights(0.0f);
    const float amount = 10.0f;
    const CellRect changed = AddToField(heights.field, {0.0f, 0.0f, 3.0f}, &amount);

    CHECK(heights.At(0, 0) == 10.0f);
    CHECK(heights.At(1, 0) == 10.0f); // inside the full-strength core
    CHECK(heights.At(2, 1) > 0.0f);
    CHECK(heights.At(2, 1) < 10.0f); // in the fade
    CHECK(heights.At(3, 0) == 0.0f); // on the rim
    for (int i = 0; i < MAP_SIZE; ++i)
    {
        CHECK(heights.At(LAST, i) == 0.0f);
        CHECK(heights.At(i, LAST) == 0.0f);
    }
    CHECK(changed.minX == 0);
    CHECK(changed.minY == 0);
    CHECK(changed.maxX == 3);
}

TEST_CASE("A brush in the open acts the same in every direction [editor][brush]")
{
    TestField heights(50.0f);
    const float amount = -8.0f;
    AddToField(heights.field, {100.0f, 60.0f, 5.0f}, &amount);
    for (int dy = -6; dy <= 6; ++dy)
    {
        for (int dx = -6; dx <= 6; ++dx)
        {
            CHECK(heights.At(100 + dx, 60 + dy) == heights.At(100 - dx, 60 - dy));
            CHECK(heights.At(100 + dx, 60 + dy) == heights.At(100 + dy, 60 + dx));
        }
    }
    CHECK(heights.At(100, 60) == 42.0f);
    CHECK(heights.At(106, 60) == 50.0f);
}

TEST_CASE("Flatten and set height land the brush's core on the target [editor][brush]")
{
    TestField heights(20.0f);
    heights.At(40, 40) = 200.0f;
    const float target = 100.0f;
    MoveFieldToward(heights.field, {40.0f, 40.0f, 4.0f}, &target, 1.0f);
    CHECK(heights.At(40, 40) == 100.0f);
    CHECK(heights.At(41, 40) == 100.0f);
    const float fading = heights.At(43, 40);
    CHECK(fading > 20.0f);
    CHECK(fading < 100.0f);
    CHECK(heights.At(44, 40) == 20.0f);

    TestField slow(0.0f);
    MoveFieldToward(slow.field, {40.0f, 40.0f, 4.0f}, &target, 0.25f);
    CHECK(slow.At(40, 40) == doctest::Approx(25.0f));
}

TEST_CASE("Smooth takes the five-point average of the values before the frame [editor][brush]")
{
    TestField heights(0.0f);
    heights.At(20, 20) = 100.0f;
    SmoothField(heights.field, {20.0f, 20.0f, 4.0f}, 1.0f);
    CHECK(heights.At(20, 20) == doctest::Approx(20.0f));
    // Each neighbour sees the spike as it was, not the already smoothed centre.
    CHECK(heights.At(19, 20) == doctest::Approx(20.0f));
    CHECK(heights.At(21, 20) == doctest::Approx(20.0f));
    CHECK(heights.At(20, 19) == doctest::Approx(20.0f));
    CHECK(heights.At(20, 21) == doctest::Approx(20.0f));
    CHECK(heights.At(21, 21) == 0.0f);

    // At the corner a missing neighbour counts as the corner itself, and the far side
    // of the map (as the old wrapping brushes read it) plays no part.
    TestField edge(5.0f);
    for (int i = 0; i < MAP_SIZE; ++i)
    {
        edge.At(LAST, i) = 1000.0f;
        edge.At(i, LAST) = 1000.0f;
    }
    SmoothField(edge.field, {0.0f, 0.0f, 3.0f}, 1.0f);
    CHECK(edge.At(0, 0) == 5.0f);
    CHECK(edge.At(1, 1) == 5.0f);
}

TEST_CASE("Light brushes act on each colour channel and clamp to the light's range [editor][brush]")
{
    TestField light(0.5f, 3);
    const float warm[3] = {0.2f, 0.1f, -0.3f};
    const CellRect changed = AddToField(light.field, {30.0f, 30.0f, 2.0f}, warm);
    CHECK(light.At(30, 30, 0) == doctest::Approx(0.7f));
    CHECK(light.At(30, 30, 1) == doctest::Approx(0.6f));
    CHECK(light.At(30, 30, 2) == doctest::Approx(0.2f));

    const float lots[3] = {5.0f, 5.0f, -5.0f};
    AddToField(light.field, {30.0f, 30.0f, 2.0f}, lots);
    ClampField(light.field, changed, 0.0f, 1.0f);
    CHECK(light.At(30, 30, 0) == 1.0f);
    CHECK(light.At(30, 30, 2) == 0.0f);
    CHECK(light.At(33, 30, 0) == 0.5f);
}

TEST_CASE("The overlay brush paints soft opacity and fades its tile out towards the rim [editor][brush]")
{
    TestOverlay overlay;
    constexpr std::uint8_t TILE = 7;
    constexpr std::uint8_t OTHER = 3;
    overlay.tiles[Index(60, 50)] = OTHER; // an old overlay at full opacity inside the brush
    overlay.alpha[Index(60, 50)] = 1.0f;
    overlay.tiles[Index(70, 50)] = OTHER; // and one well outside it
    overlay.alpha[Index(70, 50)] = 1.0f;

    PaintOverlay(overlay.layer, {60.0f, 50.0f, 4.0f}, TILE, 0.8f, 1.0f);
    CHECK(overlay.tiles[Index(60, 50)] == TILE);
    CHECK(overlay.alpha[Index(60, 50)] == doctest::Approx(0.8f));
    CHECK(overlay.tiles[Index(63, 50)] == TILE);
    CHECK(overlay.alpha[Index(63, 50)] > 0.0f);
    CHECK(overlay.alpha[Index(63, 50)] < 0.8f);
    // The first corner of the tile left of the rim takes the slot, at no opacity.
    CHECK(overlay.tiles[Index(55, 50)] == TILE);
    CHECK(overlay.alpha[Index(55, 50)] == 0.0f);
    CHECK(overlay.tiles[Index(70, 50)] == OTHER);
    CHECK(overlay.alpha[Index(70, 50)] == 1.0f);

    // Painting again moves towards the opacity, never past it.
    PaintOverlay(overlay.layer, {60.0f, 50.0f, 4.0f}, TILE, 0.8f, 1.0f);
    CHECK(overlay.alpha[Index(60, 50)] == doctest::Approx(0.8f));

    EraseOverlay(overlay.layer, {60.0f, 50.0f, 4.0f}, 1.0f);
    CHECK(overlay.tiles[Index(60, 50)] == NO_OVERLAY_TILE);
    CHECK(overlay.alpha[Index(60, 50)] == 0.0f);
    CHECK(overlay.tiles[Index(63, 50)] == TILE); // the rim only fades
    CHECK(overlay.tiles[Index(70, 50)] == OTHER);
}

TEST_CASE("The hard brush visits the tiles whose centre is inside, never off the map [editor][brush]")
{
    std::vector<std::pair<int, int>> cells;
    ForEachCellInside({10.5f, 10.5f, 1.0f}, MAP_SIZE, MAP_SIZE, CellAnchor::TileCentre,
                      [&cells](int x, int y) { cells.emplace_back(x, y); });
    std::sort(cells.begin(), cells.end());
    const std::vector<std::pair<int, int>> plus = {{9, 10}, {10, 9}, {10, 10}, {10, 11}, {11, 10}};
    CHECK(cells == plus);

    int visited = 0;
    bool onMap = true;
    ForEachCellInside({0.2f, 255.8f, 3.0f}, MAP_SIZE, MAP_SIZE, CellAnchor::TileCentre,
                      [&](int x, int y)
                      {
                          ++visited;
                          onMap = onMap && x >= 0 && x <= LAST && y >= 0 && y <= LAST;
                      });
    CHECK(visited > 0);
    CHECK(onMap);
}

TEST_CASE("A group of edits is one step: undone newest first, redone oldest first [editor][brush]")
{
    std::vector<std::string> log;
    std::vector<std::unique_ptr<EditCommand>> parts;
    parts.push_back(std::make_unique<LoggedCommand>("terrain", log));
    parts.push_back(nullptr);
    parts.push_back(std::make_unique<LoggedCommand>("objects", log));
    const std::unique_ptr<EditCommand> group = GroupEdits("Raise ground", std::move(parts));
    REQUIRE(group != nullptr);
    CHECK(group->Label() == "Raise ground");
    CHECK(group->MemoryBytes() >= 20);

    CHECK(group->Undo());
    CHECK(group->Redo());
    CHECK(log == std::vector<std::string>{"undo objects", "undo terrain", "redo terrain", "redo objects"});

    std::vector<std::unique_ptr<EditCommand>> single;
    single.push_back(std::make_unique<LoggedCommand>("terrain", log));
    const EditCommand* only = single.front().get();
    CHECK(GroupEdits("Raise ground", std::move(single)).get() == only);

    std::vector<std::unique_ptr<EditCommand>> none;
    none.push_back(nullptr);
    CHECK(GroupEdits("Raise ground", std::move(none)) == nullptr);
}
