#include <doctest.h>

#include "Editing/ObjectSelection.h"

#include <vector>

using Editor::Editing::ObjectSelection;

namespace
{
// Stand-ins for world objects: the selection only compares the pointers.
struct Objects
{
    alignas(8) unsigned char storage[8][8] = {};
    OBJECT* operator[](int i)
    {
        return reinterpret_cast<OBJECT*>(storage[i]);
    }
};
} // namespace

TEST_CASE("A click selects only the clicked object [editor][selection]")
{
    Objects objects;
    ObjectSelection selection;
    CHECK(selection.Primary() == nullptr);
    selection.SelectOnly(objects[0]);
    selection.SelectOnly(objects[1]);
    CHECK(selection.Count() == 1);
    CHECK(selection.Primary() == objects[1]);
    selection.SelectOnly(nullptr);
    CHECK(selection.IsEmpty());
}

TEST_CASE("Shift or Cmd+click adds and removes, and the last one added is the primary [editor][selection]")
{
    Objects objects;
    ObjectSelection selection;
    selection.Toggle(objects[0]);
    selection.Toggle(objects[1]);
    selection.Toggle(objects[2]);
    CHECK(selection.Count() == 3);
    CHECK(selection.Primary() == objects[2]);
    selection.Toggle(objects[2]);
    CHECK(selection.Count() == 2);
    CHECK(selection.Primary() == objects[1]);
    CHECK_FALSE(selection.Contains(objects[2]));
    selection.Add(objects[0]); // already selected: becomes the primary
    CHECK(selection.Count() == 2);
    CHECK(selection.Primary() == objects[0]);
    selection.Clear();
    CHECK(selection.IsEmpty());
}

TEST_CASE("A re-created object keeps its place in the selection [editor][selection]")
{
    Objects objects;
    ObjectSelection selection;
    selection.Assign({objects[0], objects[1], objects[2]});
    selection.Replace(objects[1], objects[5]);
    CHECK(selection.Objects() == std::vector<OBJECT*>{objects[0], objects[5], objects[2]});
    selection.Replace(objects[2], nullptr); // the re-creation failed
    CHECK(selection.Objects() == std::vector<OBJECT*>{objects[0], objects[5]});
    CHECK(selection.Primary() == objects[5]);
}

TEST_CASE("Assign leaves out nulls and repeats [editor][selection]")
{
    Objects objects;
    ObjectSelection selection;
    selection.Assign({objects[3], nullptr, objects[4], objects[3]});
    CHECK(selection.Objects() == std::vector<OBJECT*>{objects[3], objects[4]});
    CHECK(selection.Primary() == objects[4]);
}
