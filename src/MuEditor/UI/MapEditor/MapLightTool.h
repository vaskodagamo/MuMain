#pragma once

#ifdef _EDITOR

#include "MapBrushControls.h"

#include "Editing/TerrainStroke.h"

#include <string>

// The Light tab: paints the map's light map (TerrainLight, the colour the ground is
// multiplied with) with a round, soft brush, and saves it to TerrainLight.OZJ. Each
// stroke (press to release) is one undo step.
class CMapLightTool
{
public:
    enum class Mode
    {
        Add,      // adds the colour times the intensity
        Subtract, // takes it away
        Tint,     // moves the light towards the colour
        Smooth,   // evens the light out towards each corner's neighbours
    };

    // The mode, colour, intensity and radius.
    void RenderControls();
    // One frame: the [ ] keys, the outline at the cursor and, while a button is held
    // over the ground, the stroke (right-click always smooths).
    void Apply(const TerrainBrushInput& input);
    // "Save light": writes Data/World{world}/TerrainLight.OZJ and its repository copy,
    // then shows the light map exactly as the file holds it.
    void Save(int world);
    const std::string& Status() const
    {
        return m_status;
    }

    void FinishStroke();
    void CancelStroke();
    bool IsStrokeActive() const;

private:
    Editor::Editing::CellRect Paint(const Editor::Editing::BrushCircle& circle, Mode mode);

    Mode m_mode = Mode::Add;
    float m_color[3] = {1.0f, 1.0f, 1.0f};
    float m_intensity = 0.1f;
    float m_radius = 4.0f;

    Editor::Editing::TerrainStroke m_stroke;
    std::string m_status;
};

#endif // _EDITOR
