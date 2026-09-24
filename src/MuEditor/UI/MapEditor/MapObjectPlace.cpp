#include "stdafx.h"

#ifdef _EDITOR

#include "MapObjectPlace.h"
#include "MapEditorFileUtil.h"

#include "Assets/EditorText.h"
#include "Core/LiveMap.h"

#include "Engine/Object/ZzzObject.h"        // CreateObject / SaveObjects / ObjectBlock
#include "Engine/Object/w_ObjectInfo.h"     // class OBJECT (fields)
#include "Engine/Object/WorldObjectFile.h"  // EncTerrain{N}.obj records
#include "Render/Terrain/ZzzLodTerrain.h"   // RequestTerrainHeight / TERRAIN_SCALE
#include "Render/Models/ZzzBMD.h"           // BMD / Models[]
#include "Core/Globals/_enum.h"             // MODEL_WORLD_OBJECT / MAX_WORLD_OBJECTS
#include "UI/Console/MuEditorConsoleUI.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace Editor::ObjectPlace
{

std::vector<ModelEntry> EnumerateModels(int /*world*/)
{
    // Enumerate the world-object model slots the current map actually loaded,
    // rather than scanning filenames. This is map-agnostic: it covers both the
    // generic Object{N}.bmd maps and special maps like Lorencia whose objects use
    // named files (Beer01.bmd, Cannon01.bmd, ...) via a hand-coded model mapping.
    // A slot is loaded when its BMD has meshes.
    std::vector<ModelEntry> out;
    for (int type = 0; type < MAX_WORLD_OBJECTS; ++type)
    {
        const BMD& model = Models[MODEL_WORLD_OBJECT + type];
        if (model.NumMeshs <= 0 || model.Meshs == nullptr)
            continue;

        // BMD.Name is the model's internal name (ASCII). Fall back to the type
        // number if it's blank. Copy into a guaranteed null-terminated buffer.
        char narrow[sizeof(model.Name) + 1] = { 0 };
        std::memcpy(narrow, model.Name, sizeof(model.Name));
        wchar_t name[80];
        if (narrow[0] != '\0')
            swprintf_s(name, L"%hs", narrow);
        else
            swprintf_s(name, L"(type %d)", type);
        out.push_back({ type, name });
    }
    return out;
}

float GroundHeightAt(float x, float y)
{
    return RequestTerrainHeight(x, y);
}

void ComputePlacementPosition(float x, float y, bool snap, vec3_t outPos)
{
    float px = x;
    float py = y;
    if (snap)
    {
        px = ((int)(x / TERRAIN_SCALE) + 0.5f) * TERRAIN_SCALE;
        py = ((int)(y / TERRAIN_SCALE) + 0.5f) * TERRAIN_SCALE;
    }
    outPos[0] = px;
    outPos[1] = py;
    outPos[2] = GroundHeightAt(px, py);
}

bool Save(int world, std::string& outReport, Editor::Files::SavedFile* outSaved)
{
    const std::filesystem::path fileName = Editor::Files::TerrainObjectFile(world);
    std::wstring saveName = fileName.wstring(); // SaveObjects takes a mutable buffer
    if (!SaveObjects(saveName.data(), world))
    {
        outReport = "Save FAILED: could not write " + Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(fileName)) +
                    " (see MuError.log).";
        g_MuEditorConsoleUI.LogEditor("[MapEditor] SaveObjects FAILED");
        return false;
    }
    const Editor::Files::SavedFile saved = Editor::Files::MirrorSavedFile(fileName);
    outReport = Editor::Files::DescribeSavedFiles({saved});
    if (outSaved != nullptr)
        *outSaved = saved;
    Editor::LiveMap::NoteSaved(Editor::MapInspect::SaveUnit::Objects, world);
    return true;
}

OBJECT* PickUnderCursor()
{
    return CollisionDetectObjects(nullptr);
}

namespace
{
    // World units spanned by one 16x16 object-grid block.
    constexpr float BLOCK_SPAN = 16.0f * TERRAIN_SCALE;

    int BlockOf(float x, float y)
    {
        const int i = (int)(x / BLOCK_SPAN);
        const int j = (int)(y / BLOCK_SPAN);
        if (i < 0 || j < 0 || i >= 16 || j >= 16)
            return -1;
        return i * 16 + j;
    }
}

OBJECT* Reposition(OBJECT* o, float x, float y, float z)
{
    if (o == nullptr)
        return nullptr;

    const int newBlock = BlockOf(x, y);
    if (newBlock < 0)
        return o;  // outside the placeable grid; ignore the move

    if ((BYTE)newBlock == o->Block)
    {
        o->Position[0] = x; o->Position[1] = y; o->Position[2] = z;
        o->StartPosition[0] = x; o->StartPosition[1] = y; o->StartPosition[2] = z;
        return o;
    }

    // Crossed a block boundary: re-create in the correct block, carrying the
    // object's type/angle/scale, then delete the old instance.
    vec3_t pos = { x, y, z };
    vec3_t ang; VectorCopy(o->Angle, ang);
    const int   type  = o->Type;
    const float scale = o->Scale;
    OBJECT* created = CreateObject(type, pos, ang, scale);
    if (created != nullptr)
        created->SaveOrder = o->SaveOrder;
    DeleteObject(o, &ObjectBlock[o->Block]);
    return created;
}

void Remove(OBJECT* o)
{
    if (o != nullptr)
        DeleteObject(o, &ObjectBlock[o->Block]);
}

namespace
{
    bool ByYThenX(const OBJECT* a, const OBJECT* b)
    {
        if (a->Position[1] != b->Position[1])
            return a->Position[1] < b->Position[1];
        return a->Position[0] < b->Position[0];
    }
}

void ForEachLiveObject(const std::function<void(OBJECT*)>& visit)
{
    for (OBJECT_BLOCK& block : ObjectBlock)
    {
        for (OBJECT* o = block.Head; o != nullptr; o = o->Next)
        {
            if (o->Live)
                visit(o);
        }
    }
}

std::vector<OBJECT*> LiveObjectsOfType(int type)
{
    std::vector<OBJECT*> found;
    ForEachLiveObject(
        [type, &found](OBJECT* o)
        {
            if (o->Type == type)
                found.push_back(o);
        });
    std::sort(found.begin(), found.end(), ByYThenX);
    return found;
}

void CountLiveObjects(std::vector<int>& counts)
{
    std::fill(counts.begin(), counts.end(), 0);
    ForEachLiveObject(
        [&counts](const OBJECT* o)
        {
            if (o->Type < 0)
                return;
            if (static_cast<std::size_t>(o->Type) >= counts.size())
                counts.resize(static_cast<std::size_t>(o->Type) + 1, 0);
            ++counts[o->Type];
        });
}

int FindRecordIndex(const std::filesystem::path& objFile, int type, const vec3_t position)
{
    std::vector<unsigned char> data = Editor::Files::ReadWholeFile(objFile);
    if (data.empty())
        return -1;
    const int size = static_cast<int>(data.size());
    std::vector<unsigned char> plain(data.size());
    MapFileDecrypt(plain.data(), data.data(), size);

    namespace ObjectFile = Engine::Object::WorldObjectFile;
    ObjectFile::Contents contents;
    ObjectFile::Decode(plain.data(), plain.size(), contents); // a cut-short file still yields its complete records
    for (std::size_t i = 0; i < contents.records.size(); ++i)
    {
        const ObjectFile::Record& record = contents.records[i];
        const bool samePlace =
            record.position[0] == position[0] && record.position[1] == position[1] && record.position[2] == position[2];
        if (record.type == type && samePlace)
            return static_cast<int>(i);
    }
    return -1;
}

std::string ModelName(int type)
{
    const BMD& model = Models[type];
    char narrow[sizeof(model.Name) + 1] = {};
    std::memcpy(narrow, model.Name, sizeof(model.Name));
    if (narrow[0] != '\0')
        return Editor::Text::ValidUtf8(narrow); // many are Korean (CP949) bytes; JSON and ImGui take UTF-8

    char text[32];
    std::snprintf(text, sizeof(text), "(type %d)", type);
    return text;
}

} // namespace Editor::ObjectPlace

#endif // _EDITOR
