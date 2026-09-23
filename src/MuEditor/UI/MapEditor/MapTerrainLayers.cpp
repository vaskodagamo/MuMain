#include "stdafx.h"

#ifdef _EDITOR

#include "MapTerrainLayers.h"

#include "Render/Terrain/ZzzLodTerrain.h" // TerrainWall

extern unsigned char TerrainMappingLayer1[];
extern unsigned char TerrainMappingLayer2[];
extern float TerrainMappingAlpha[];
extern float BackTerrainHeight[];

int CMapTerrainLayers::Width() const
{
    return TERRAIN_SIZE;
}

int CMapTerrainLayers::Height() const
{
    return TERRAIN_SIZE;
}

std::size_t CMapTerrainLayers::ElementBytes(int layer) const
{
    switch (layer)
    {
    case MAP_LAYER_TILE_BASE:
        return sizeof(TerrainMappingLayer1[0]);
    case MAP_LAYER_TILE_OVERLAY:
        return sizeof(TerrainMappingLayer2[0]);
    case MAP_LAYER_TILE_ALPHA:
        return sizeof(TerrainMappingAlpha[0]);
    case MAP_LAYER_HEIGHT:
        return sizeof(BackTerrainHeight[0]);
    case MAP_LAYER_WALL:
        return sizeof(TerrainWall[0]);
    case MAP_LAYER_LIGHT:
        return sizeof(TerrainLight[0]);
    default:
        return 0;
    }
}

std::uint8_t* CMapTerrainLayers::Data(int layer)
{
    switch (layer)
    {
    case MAP_LAYER_TILE_BASE:
        return reinterpret_cast<std::uint8_t*>(TerrainMappingLayer1);
    case MAP_LAYER_TILE_OVERLAY:
        return reinterpret_cast<std::uint8_t*>(TerrainMappingLayer2);
    case MAP_LAYER_TILE_ALPHA:
        return reinterpret_cast<std::uint8_t*>(TerrainMappingAlpha);
    case MAP_LAYER_HEIGHT:
        return reinterpret_cast<std::uint8_t*>(BackTerrainHeight);
    case MAP_LAYER_WALL:
        return reinterpret_cast<std::uint8_t*>(TerrainWall);
    case MAP_LAYER_LIGHT:
        return reinterpret_cast<std::uint8_t*>(TerrainLight);
    default:
        return nullptr;
    }
}

void CMapTerrainLayers::RelightHeights(const Editor::Editing::CellRect& heights)
{
    if (heights.IsEmpty())
        return;
    // A normal reads its own corner's height and the three towards +x/+y, so a changed
    // height reaches the normals one cell towards -x/-y; one more on the other sides
    // keeps the rectangle symmetric at no real cost.
    constexpr int NORMAL_REACH = 1;
    const int minX = heights.minX - NORMAL_REACH;
    const int minY = heights.minY - NORMAL_REACH;
    const int maxX = heights.maxX + NORMAL_REACH;
    const int maxY = heights.maxY + NORMAL_REACH;
    CreateTerrainNormal_Rect(minX, minY, maxX, maxY);
    CreateTerrainLight_Rect(minX, minY, maxX, maxY);
}

void CMapTerrainLayers::RelightCells(const Editor::Editing::CellRect& cells)
{
    if (!cells.IsEmpty())
        CreateTerrainLight_Rect(cells.minX, cells.minY, cells.maxX, cells.maxY);
}

void CMapTerrainLayers::Changed(int layer, const Editor::Editing::CellRect& rect)
{
    if (layer == MAP_LAYER_HEIGHT)
        RelightHeights(rect);
    else if (layer == MAP_LAYER_LIGHT)
        RelightCells(rect);
    else if (layer == MAP_LAYER_WALL)
        m_wallsChanged = true;
}

bool CMapTerrainLayers::TakeWallsChanged()
{
    const bool changed = m_wallsChanged;
    m_wallsChanged = false;
    return changed;
}

#endif // _EDITOR
