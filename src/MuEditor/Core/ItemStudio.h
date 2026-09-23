#pragma once

#ifdef _EDITOR

// The Item Editor's studio: "Main --editor --items" (without --world) shows no
// map at all - a plain dark backdrop, no terrain, objects, sky, weather, effects
// or fog - and the Item Editor fills the window under the MU Editor toolbar.
//
// The engine's main scene still runs with World1 loaded (OfflineWorld): the Map
// Editor edits and saves the loaded map, so it needs one when it is opened from
// the toolbar, and the Item Editor's preview puts its dropped item and dressed
// character on the map's ground and light at the hidden hero's spot. Only drawing is skipped: while the Map Editor is
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
// object pass (see UI/MapEditor/ObjectThumbnail.h), and the Item Editor's preview
// (UI/ItemEditor/ItemPreview.h).
void RenderInsteadOfWorld();

// Called by the main scene after it drew the world: renders the Item Editor's
// preview when the map is shown (the Map Editor is open, or --items --world N).
void RenderAfterWorld();
} // namespace Editor::ItemStudio

#endif // _EDITOR
