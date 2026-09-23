#include "App/stdafx.h"

#include <doctest.h>

#include "Engine/Object/ObjectReferences.h"
#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"

#include <unordered_set>

extern int SelectedOperate; // ZzzInterface.cpp

namespace
{
constexpr int NO_OPERATE = -1;

// Slots used below; any free slot works.
constexpr int CHAIR_OPERATE = 3;
constexpr int TREE_OPERATE = 4;
constexpr int CHAIR_EFFECT = 5;
constexpr int TREE_EFFECT = 6;
constexpr int CHAIR_JOINT = 7;
constexpr int CHAIR_PARTICLE = 8;
constexpr int TREE_PARTICLE = 9;

void ClearArrays()
{
    for (int i = 0; i < MAX_OPERATES; ++i)
        Operates[i] = {};
    for (int i = 0; i < MAX_EFFECTS; ++i)
    {
        Effects[i].Live = false;
        Effects[i].Owner = nullptr;
    }
    for (int i = 0; i < MAX_JOINTS; ++i)
    {
        Joints[i].Live = false;
        Joints[i].Target = nullptr;
    }
    for (int i = 0; i < MAX_PARTICLES; ++i)
    {
        Particles[i].Live = false;
        Particles[i].Target = nullptr;
    }
    SelectedOperate = NO_OPERATE;
}

void AttachTo(OBJECT* chair, OBJECT* tree)
{
    Operates[CHAIR_OPERATE] = {true, 0, chair};
    Operates[TREE_OPERATE] = {true, 0, tree};
    Effects[CHAIR_EFFECT].Live = true;
    Effects[CHAIR_EFFECT].Owner = chair;
    Effects[TREE_EFFECT].Live = true;
    Effects[TREE_EFFECT].Owner = tree;
    Joints[CHAIR_JOINT].Live = true;
    Joints[CHAIR_JOINT].Target = chair;
    Particles[CHAIR_PARTICLE].Live = true;
    Particles[CHAIR_PARTICLE].Target = chair;
    Particles[TREE_PARTICLE].Live = true;
    Particles[TREE_PARTICLE].Target = tree;
}

int LiveOperates()
{
    int live = 0;
    for (int i = 0; i < MAX_OPERATES; ++i)
    {
        if (Operates[i].Live)
            ++live;
    }
    return live;
}
} // namespace

TEST_CASE("Freeing an object drops the operates and effects that point at it [engine][objects]")
{
    ClearArrays();
    OBJECT chair;
    OBJECT tree;
    AttachTo(&chair, &tree);
    SelectedOperate = CHAIR_OPERATE;

    Engine::Object::ReleaseReferencesTo(&chair);

    CHECK_FALSE(Operates[CHAIR_OPERATE].Live);
    CHECK(Operates[CHAIR_OPERATE].Owner == nullptr);
    CHECK(SelectedOperate == NO_OPERATE);
    CHECK_FALSE(Effects[CHAIR_EFFECT].Live);
    CHECK_FALSE(Joints[CHAIR_JOINT].Live);
    CHECK_FALSE(Particles[CHAIR_PARTICLE].Live);

    CHECK(Operates[TREE_OPERATE].Live);
    CHECK(Operates[TREE_OPERATE].Owner == &tree);
    CHECK(Effects[TREE_EFFECT].Live);
    CHECK(Particles[TREE_PARTICLE].Live);
    CHECK(LiveOperates() == 1);
}

TEST_CASE("The operate selection survives releasing another object [engine][objects]")
{
    ClearArrays();
    OBJECT chair;
    OBJECT tree;
    AttachTo(&chair, &tree);
    SelectedOperate = TREE_OPERATE;

    Engine::Object::ReleaseReferencesTo(&chair);

    CHECK(SelectedOperate == TREE_OPERATE);
}

TEST_CASE("Freeing all objects at once drops every reference to them [engine][objects]")
{
    ClearArrays();
    OBJECT chair;
    OBJECT tree;
    OBJECT unrelated;
    AttachTo(&chair, &tree);
    Particles[0].Live = true;
    Particles[0].Target = &unrelated;

    Engine::Object::ReleaseReferencesTo(std::unordered_set<const OBJECT*>{&chair, &tree});

    CHECK(LiveOperates() == 0);
    CHECK_FALSE(Effects[CHAIR_EFFECT].Live);
    CHECK_FALSE(Effects[TREE_EFFECT].Live);
    CHECK_FALSE(Joints[CHAIR_JOINT].Live);
    CHECK_FALSE(Particles[CHAIR_PARTICLE].Live);
    CHECK_FALSE(Particles[TREE_PARTICLE].Live);
    CHECK(Particles[0].Live);
}
