#include <doctest.h>

#include "Editing/PopupMouseGuard.h"

using Editor::Editing::PopupMouseGuard;

TEST_CASE("The world gets the mouse while no popup is open [editor][popup]")
{
    PopupMouseGuard guard;
    CHECK_FALSE(guard.Update(false, false));
    CHECK_FALSE(guard.Update(false, true)); // a click on the world
    CHECK_FALSE(guard.Update(false, false));
}

TEST_CASE("An open popup keeps the mouse, also without a button held [editor][popup]")
{
    PopupMouseGuard guard;
    CHECK(guard.Update(true, false));
    CHECK(guard.Update(true, false));
    CHECK_FALSE(guard.Update(false, false)); // closed from inside: the next click is the world's
}

TEST_CASE("The click that closes a popup stays with it until it is released [editor][popup]")
{
    PopupMouseGuard guard;
    CHECK(guard.Update(true, false));
    CHECK(guard.Update(true, true)); // the press outside the popup; ImGui closes it this frame
    CHECK(guard.Update(false, true));
    CHECK(guard.Update(false, true)); // still held over the world: no paint, no pick
    CHECK_FALSE(guard.Update(false, false));
    CHECK_FALSE(guard.Update(false, true)); // the next press is a normal click again
}

TEST_CASE("A popup opened by a press keeps the mouse until that press ends [editor][popup]")
{
    PopupMouseGuard guard;
    CHECK_FALSE(guard.Update(false, true)); // pressing a combo; its list opens this frame
    CHECK(guard.Update(true, true));
    CHECK(guard.Update(true, false)); // released on an item: the list closes this frame
    CHECK_FALSE(guard.Update(false, false));
}
