#pragma once

#ifdef _EDITOR

// The Item Editor's studio: "Main --editor --items" (without --world) shows no
// map at all - a plain dark backdrop, no terrain, objects, sky, weather, effects
// or fog - and the Item Editor fills the window under the MU Editor toolbar.
//
// The engine's main scene still runs with World1 loaded (OfflineWorld): the Map
// Editor edits and saves the loaded map, so it needs one when it is opened from
// the toolbar, and the hidden hero (the Item Editor's equipped preview) stands
// on a map's ground and light. Only drawing is skipped: while the Map Editor is
// open, or the Item Editor is closed, the map is drawn as usual.
namespace Editor::ItemStudio
{
struct Color
{
    float r;
    float g;
    float b;
};

// The backdrop: a flat dark grey, a little lighter than the editor's windows.
constexpr Color BACKDROP_COLOR{0.17f, 0.17f, 0.18f};

// True while the studio shows its backdrop instead of the map this frame.
bool ShowsBackdropOnly();

// True while the Item Editor window fills the client area (studio, Map Editor closed).
bool DocksItemEditor();

// Called by the main scene instead of drawing the world while ShowsBackdropOnly():
// renders the queued model thumbnails, which otherwise render during the world's
// object pass (see UI/MapEditor/ObjectThumbnail.h).
void RenderInsteadOfWorld();
} // namespace Editor::ItemStudio

#endif // _EDITOR
