#pragma once

#ifdef _EDITOR

#include "MapBrushControls.h"
#include "MapGroundFollowers.h"

#include "Editing/TerrainStroke.h"

#include <string>

// The Height tab's round brush: raise/lower, flatten, smooth and set to a height,
// with a soft edge, stopping at the map's edges. Each stroke (press to release) is
// one undo step, together with the objects that followed the ground.
class CMapHeightTool
{
public:
    enum class Tool
    {
        RaiseLower, // left raises, right lowers
        Flatten,    // towards the height under the cursor where the stroke began
        Smooth,     // towards the average of each corner's neighbours
        SetHeight,  // towards the target height (Alt-click takes it from the ground)
    };

    // The tool choice, radius, strength, target and "Objects follow terrain".
    void RenderControls();
    // One frame: the [ ] keys, the outline at the cursor and, while a button is held
    // over the ground, the stroke.
    void Apply(const TerrainBrushInput& input);
    // Ends a held stroke as one undo step.
    void FinishStroke();
    // Ends a held stroke without an undo step (its map was unloaded).
    void CancelStroke();
    bool IsStrokeActive() const;

private:
    void BeginStroke(const TerrainBrushInput& input, bool lower);
    // One frame of the stroke; returns the corners it may have changed.
    Editor::Editing::CellRect Sculpt(const Editor::Editing::BrushCircle& circle, bool lower);
    // Strength as the share of the gap Flatten, Smooth and Set height close per frame.
    float Rate() const;
    bool SamplesTarget(const TerrainBrushInput& input);

    Tool m_tool = Tool::RaiseLower;
    float m_radius = 3.0f;
    float m_strength = 8.0f;
    float m_targetHeight = 100.0f;
    bool m_followObjects = true;

    Editor::Editing::TerrainStroke m_stroke;
    std::string m_strokeLabel;
    float m_flattenHeight = 0.0f; // the stroke's Flatten target
    CMapGroundFollowers m_followers;
};

#endif // _EDITOR
