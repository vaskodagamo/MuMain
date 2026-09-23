#pragma once

#ifdef _EDITOR

#include "GizmoMath.h" // Vec3

// Where the editor's model thumbnails (UI/MapEditor/ObjectThumbnail) put the
// camera so the model fills the picture, from the model's bounding box (MU
// models are Z-up). Pure math, no renderer.
namespace Editor::Thumbnail
{
enum class Framing
{
    // World objects: a 3/4 view from above with room around the model (the Map
    // Editor's object palette).
    Object,
    // Items: the broad side of the model (the flat of a blade, the face of a
    // shield or a wing), long items laid corner to corner, framed tightly so a
    // long spear and a small ring both fill the picture.
    Item,
};

struct Camera
{
    Gizmo::Vec3 eye;
    Gizmo::Vec3 center;
    Gizmo::Vec3 up;
    float fovDegrees = 0.0f; // vertical = horizontal: thumbnails are square
    float zNear = 0.0f;
    float zFar = 0.0f;
};

Camera FrameBounds(const Gizmo::Vec3& boundsMin, const Gizmo::Vec3& boundsMax, Framing framing);
} // namespace Editor::Thumbnail

#endif // _EDITOR
