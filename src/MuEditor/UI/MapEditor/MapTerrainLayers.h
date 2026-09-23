#pragma once

#ifdef _EDITOR

#include "Editing/TerrainLayers.h"

// The terrain arrays the Map Editor's brushes paint, as layers for the undo history.
enum MapTerrainLayer
{
    MAP_LAYER_TILE_BASE,    // TerrainMappingLayer1, one byte per cell
    MAP_LAYER_TILE_OVERLAY, // TerrainMappingLayer2, one byte per cell
    MAP_LAYER_TILE_ALPHA,   // TerrainMappingAlpha, one float per cell
    MAP_LAYER_HEIGHT,       // BackTerrainHeight, one float per cell
    MAP_LAYER_WALL,         // TerrainWall, one WORD per cell
    MAP_LAYER_LIGHT,        // TerrainLight, the painted light map, three floats per cell
    MAP_LAYER_COUNT,
};

// The live terrain of the loaded map (256x256 cells). After an undo or redo writes the
// heights or the light map, the lighting of the cells around them is rebuilt, exactly
// as after a stroke.
class CMapTerrainLayers final : public Editor::Editing::TerrainLayers
{
public:
    // Rebuilds the normals and the lit colours of every cell whose normal reads a height
    // in `heights` (the rectangle grown by one cell, across the map's edges as the
    // normals wrap), the same values a whole-map rebuild gives them.
    static void RelightHeights(const Editor::Editing::CellRect& heights);
    // Rebuilds the lit colours of `cells` after their light map changed.
    static void RelightCells(const Editor::Editing::CellRect& cells);

    int Width() const override;
    int Height() const override;
    std::size_t ElementBytes(int layer) const override;
    std::uint8_t* Data(int layer) override;
    void Changed(int layer, const Editor::Editing::CellRect& rect) override;

    // True once after an undo or redo changed the walkability (the Attribute tab
    // then counts its edited tiles again).
    bool TakeWallsChanged();

private:
    bool m_wallsChanged = false;
};

#endif // _EDITOR
