#include <doctest.h>

#include "Editing/CommandStack.h"
#include "Editing/ObjectEditCommand.h"
#include "Editing/TerrainPatchCommand.h"
#include "Editing/TerrainStroke.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace Editor::Editing;

namespace
{
// A command that counts how it was applied and claims a fixed size.
class CountingCommand : public EditCommand
{
public:
    CountingCommand(std::string label, std::vector<std::string>& log, std::size_t bytes = 100, bool fails = false)
        : EditCommand(std::move(label)), m_log(log), m_bytes(bytes), m_fails(fails)
    {
    }
    bool Undo() override
    {
        m_log.push_back("undo " + Label());
        return !m_fails;
    }
    bool Redo() override
    {
        m_log.push_back("redo " + Label());
        return !m_fails;
    }
    std::size_t MemoryBytes() const override
    {
        return m_bytes;
    }

private:
    std::vector<std::string>& m_log;
    std::size_t m_bytes;
    bool m_fails;
};

// World objects by key, as the engine adapter keeps them by OBJECT::SaveOrder.
class FakeObjectWorld : public ObjectWorld
{
public:
    bool Create(int key, const ObjectState& state) override
    {
        if (objects.count(key) != 0)
            return false;
        objects[key] = state;
        ++creations;
        return true;
    }
    bool Remove(int key) override
    {
        return objects.erase(key) == 1;
    }
    bool Update(int key, const ObjectState& state) override
    {
        auto found = objects.find(key);
        if (found == objects.end())
            return false;
        found->second = state;
        return true;
    }

    std::map<int, ObjectState> objects;
    int creations = 0;
};

ObjectState MakeState(int type, float x, float y, float yaw = 0.0f, float scale = 1.0f)
{
    ObjectState state;
    state.type = type;
    state.position[0] = x;
    state.position[1] = y;
    state.position[2] = 170.0f;
    state.angle[2] = yaw;
    state.scale = scale;
    return state;
}

// Two layers over a small grid: one byte per cell and one float per cell.
class FakeTerrain : public TerrainLayers
{
public:
    static constexpr int WIDTH = 8;
    static constexpr int HEIGHT = 6;
    static constexpr int BYTE_LAYER = 0;
    static constexpr int FLOAT_LAYER = 1;

    FakeTerrain() : bytes(WIDTH * HEIGHT, 7), floats(WIDTH * HEIGHT, 1.5f) {}
    int Width() const override
    {
        return WIDTH;
    }
    int Height() const override
    {
        return HEIGHT;
    }
    std::size_t ElementBytes(int layer) const override
    {
        return layer == BYTE_LAYER ? sizeof(std::uint8_t) : sizeof(float);
    }
    std::uint8_t* Data(int layer) override
    {
        return layer == BYTE_LAYER ? bytes.data() : reinterpret_cast<std::uint8_t*>(floats.data());
    }
    void Changed(int layer, const CellRect& rect) override
    {
        changedLayers.push_back(layer);
        lastRect = rect;
    }
    std::uint8_t& Byte(int x, int y)
    {
        return bytes[y * WIDTH + x];
    }
    float& Float(int x, int y)
    {
        return floats[y * WIDTH + x];
    }

    std::vector<std::uint8_t> bytes;
    std::vector<float> floats;
    std::vector<int> changedLayers;
    CellRect lastRect;
};

bool SameRect(const CellRect& rect, int minX, int minY, int maxX, int maxY)
{
    return rect.minX == minX && rect.minY == minY && rect.maxX == maxX && rect.maxY == maxY;
}
} // namespace

TEST_CASE("CommandStack undoes and redoes in order and names the next step [editor][history]")
{
    std::vector<std::string> log;
    CommandStack stack;
    CHECK_FALSE(stack.CanUndo());
    CHECK(stack.UndoLabel().empty());
    CHECK(stack.Undo() == StepResult::Nothing);

    stack.Push(std::make_unique<CountingCommand>("Paint texture", log));
    stack.Push(std::make_unique<CountingCommand>("Move 3 objects", log));
    CHECK(stack.UndoLabel() == "Move 3 objects");
    CHECK(stack.Undo() == StepResult::Done);
    CHECK(stack.UndoLabel() == "Paint texture");
    CHECK(stack.RedoLabel() == "Move 3 objects");
    CHECK(stack.Undo() == StepResult::Done);
    CHECK_FALSE(stack.CanUndo());
    CHECK(stack.Redo() == StepResult::Done);
    CHECK(stack.Redo() == StepResult::Done);
    CHECK(stack.Redo() == StepResult::Nothing);
    CHECK(log == std::vector<std::string>{"undo Move 3 objects", "undo Paint texture", "redo Paint texture",
                                          "redo Move 3 objects"});
}

TEST_CASE("CommandStack lists every step's label, undo oldest first and redo next first [editor][history]")
{
    std::vector<std::string> log;
    CommandStack stack;
    CHECK(stack.UndoLabels().empty());
    CHECK(stack.RedoLabels().empty());
    stack.Push(std::make_unique<CountingCommand>("Hill", log));
    stack.Push(std::make_unique<CountingCommand>("Road", log));
    stack.Push(std::make_unique<CountingCommand>("Trees", log));
    CHECK(stack.UndoLabels() == std::vector<std::string>{"Hill", "Road", "Trees"});
    stack.Undo();
    stack.Undo();
    CHECK(stack.UndoLabels() == std::vector<std::string>{"Hill"});
    CHECK(stack.RedoLabels() == std::vector<std::string>{"Road", "Trees"});
    CHECK(stack.RedoLabels().front() == stack.RedoLabel());
}

TEST_CASE("CommandStack drops the redo side when a new step is pushed [editor][history]")
{
    std::vector<std::string> log;
    CommandStack stack;
    stack.Push(std::make_unique<CountingCommand>("A", log));
    stack.Push(std::make_unique<CountingCommand>("B", log));
    stack.Undo();
    CHECK(stack.CanRedo());
    stack.Push(std::make_unique<CountingCommand>("C", log));
    CHECK_FALSE(stack.CanRedo());
    CHECK(stack.UndoCount() == 2);
    CHECK(stack.UndoLabel() == "C");
    CHECK(stack.MemoryBytes() == 200);
    stack.Push(nullptr); // an edit that changed nothing
    CHECK(stack.UndoCount() == 2);
}

TEST_CASE("CommandStack drops the oldest steps over its memory limit, never the newest [editor][history]")
{
    std::vector<std::string> log;
    CommandStack stack(250);
    stack.Push(std::make_unique<CountingCommand>("A", log, 100));
    stack.Push(std::make_unique<CountingCommand>("B", log, 100));
    stack.Push(std::make_unique<CountingCommand>("C", log, 100));
    CHECK(stack.UndoCount() == 2);
    CHECK(stack.MemoryBytes() == 200);
    stack.Undo();
    stack.Undo();
    CHECK_FALSE(stack.CanUndo());
    CHECK(log == std::vector<std::string>{"undo C", "undo B"});

    CommandStack small(50);
    small.Push(std::make_unique<CountingCommand>("Huge stroke", log, 1000));
    CHECK(small.UndoCount() == 1);
    CHECK(small.UndoLabel() == "Huge stroke");
}

TEST_CASE("CommandStack clears itself when a step no longer matches [editor][history]")
{
    std::vector<std::string> log;
    CommandStack stack;
    stack.Push(std::make_unique<CountingCommand>("A", log));
    stack.Push(std::make_unique<CountingCommand>("Broken", log, 100, true));
    CHECK(stack.Undo() == StepResult::Failed);
    CHECK_FALSE(stack.CanUndo());
    CHECK_FALSE(stack.CanRedo());
    CHECK(stack.MemoryBytes() == 0);
}

TEST_CASE("Object commands create, delete and move objects both ways by key [editor][history]")
{
    FakeObjectWorld world;
    world.objects[0] = MakeState(10, 100.0f, 200.0f);
    world.objects[1] = MakeState(11, 300.0f, 400.0f);
    CommandStack stack;

    // Move both (a batch), then delete object 0, then place a new object 7.
    std::vector<KeyedObjectState> before = {{0, world.objects[0]}, {1, world.objects[1]}};
    std::vector<KeyedObjectState> after = before;
    after[0].state.position[0] += 50.0f;
    after[1].state.angle[2] = 45.0f;
    world.objects[0] = after[0].state;
    world.objects[1] = after[1].state;
    stack.Push(std::make_unique<ObjectEditCommand>("Move 2 objects", world, TransformChanges(before, after)));

    const std::vector<KeyedObjectState> removed = {{0, world.objects[0]}};
    world.objects.erase(0);
    stack.Push(std::make_unique<ObjectEditCommand>("Delete object", world, RemovalChanges(removed)));

    const std::vector<KeyedObjectState> created = {{7, MakeState(12, 500.0f, 600.0f)}};
    world.objects[7] = created[0].state;
    stack.Push(std::make_unique<ObjectEditCommand>("Place object", world, CreationChanges(created)));

    CHECK(stack.Undo() == StepResult::Done); // place
    CHECK(world.objects.count(7) == 0);
    CHECK(stack.Undo() == StepResult::Done); // delete: object 0 comes back under its key
    REQUIRE(world.objects.count(0) == 1);
    CHECK(world.objects[0] == after[0].state);
    CHECK(stack.Undo() == StepResult::Done); // the move, which names object 0 again
    CHECK(world.objects[0] == before[0].state);
    CHECK(world.objects[1] == before[1].state);

    CHECK(stack.Redo() == StepResult::Done);
    CHECK(stack.Redo() == StepResult::Done);
    CHECK(stack.Redo() == StepResult::Done);
    CHECK(world.objects.count(0) == 0);
    CHECK(world.objects[1] == after[1].state);
    CHECK(world.objects[7] == created[0].state);
    CHECK(world.objects.size() == 2);
    // Only the deleted object (undo) and the placed one (redo) were created, never the rest.
    CHECK(world.creations == 2);
}

TEST_CASE("TransformChanges leaves out objects that did not change [editor][history]")
{
    const std::vector<KeyedObjectState> before = {{3, MakeState(1, 10.0f, 10.0f)}, {4, MakeState(1, 20.0f, 20.0f)}};
    std::vector<KeyedObjectState> after = before;
    after[1].state.scale = 2.0f;
    const std::vector<ObjectChange> changes = TransformChanges(before, after);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].key == 4);
    CHECK(changes[0].before->scale == 1.0f);
    CHECK(changes[0].after->scale == 2.0f);
    CHECK(TransformChanges(before, before).empty());
}

TEST_CASE("An object command reports an object that is gone [editor][history]")
{
    FakeObjectWorld world;
    world.objects[5] = MakeState(1, 0.0f, 0.0f);
    std::vector<KeyedObjectState> before = {{5, world.objects[5]}};
    std::vector<KeyedObjectState> after = before;
    after[0].state.position[1] = 99.0f;
    ObjectEditCommand command("Move object", world, TransformChanges(before, after));
    world.objects.clear();
    CHECK_FALSE(command.Undo());
    CHECK_FALSE(command.Redo());
}

TEST_CASE("A terrain stroke becomes one step over the rectangle it changed [editor][history]")
{
    FakeTerrain terrain;
    const std::vector<std::uint8_t> bytesBefore = terrain.bytes;
    const std::vector<float> floatsBefore = terrain.floats;

    TerrainStroke stroke;
    stroke.Begin(terrain, {FakeTerrain::BYTE_LAYER, FakeTerrain::FLOAT_LAYER}, "Paint texture");
    CHECK(stroke.IsActive());
    terrain.Byte(2, 1) = 3;
    terrain.Byte(4, 3) = 9;
    terrain.Float(5, 2) = 0.25f;
    std::unique_ptr<EditCommand> command = stroke.Finish();
    CHECK_FALSE(stroke.IsActive());
    REQUIRE(command);
    CHECK(command->Label() == "Paint texture");
    const auto* patch = static_cast<const TerrainPatchCommand*>(command.get());
    CHECK(SameRect(patch->Rect(), 2, 1, 5, 3));
    const std::vector<std::uint8_t> bytesAfter = terrain.bytes;
    const std::vector<float> floatsAfter = terrain.floats;

    REQUIRE(command->Undo());
    CHECK(terrain.bytes == bytesBefore);
    CHECK(terrain.floats == floatsBefore);
    CHECK(terrain.changedLayers == std::vector<int>{FakeTerrain::BYTE_LAYER, FakeTerrain::FLOAT_LAYER});
    CHECK(SameRect(terrain.lastRect, 2, 1, 5, 3));

    REQUIRE(command->Redo());
    CHECK(terrain.bytes == bytesAfter);
    CHECK(terrain.floats == floatsAfter);
}

TEST_CASE("A terrain step keeps only its rectangle, and an empty stroke none [editor][history]")
{
    FakeTerrain terrain;
    TerrainStroke stroke;
    stroke.Begin(terrain, {FakeTerrain::FLOAT_LAYER}, "Raise ground");
    CHECK(stroke.Finish() == nullptr); // nothing painted

    stroke.Begin(terrain, {FakeTerrain::FLOAT_LAYER}, "Raise ground");
    terrain.Float(0, 0) = 9.0f;
    terrain.Float(1, 0) = 9.0f;
    std::unique_ptr<EditCommand> small = stroke.Finish();
    REQUIRE(small);

    stroke.Begin(terrain, {FakeTerrain::FLOAT_LAYER}, "Raise ground");
    terrain.Float(0, 0) = 1.0f;
    terrain.Float(FakeTerrain::WIDTH - 1, FakeTerrain::HEIGHT - 1) = 1.0f;
    std::unique_ptr<EditCommand> whole = stroke.Finish();
    REQUIRE(whole);
    CHECK(whole->MemoryBytes() > small->MemoryBytes());
    const std::size_t wholeLayerBytes = 2 * FakeTerrain::WIDTH * FakeTerrain::HEIGHT * sizeof(float);
    CHECK(whole->MemoryBytes() >= wholeLayerBytes);

    stroke.Begin(terrain, {FakeTerrain::BYTE_LAYER}, "Paint texture");
    terrain.Byte(3, 3) = 1;
    stroke.Cancel();
    CHECK_FALSE(stroke.IsActive());
    CHECK(stroke.Finish() == nullptr);
}
