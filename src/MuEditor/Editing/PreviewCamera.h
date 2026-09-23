#pragma once

#ifdef _EDITOR

#include "GizmoMath.h" // Vec3

// The camera of the Item Editor's 3D preview (UI/ItemEditor/ItemPreview): an
// orbit around a point (the item, the dropped item, the dressed character) that
// the mouse turns and zooms, with an optional slow turn of its own. MU models
// are Z-up, so the orbit turns around the world Z axis and tilts up and down.
// Pure math, no renderer.
namespace Editor::Preview
{
// Where the camera sits around the point it looks at.
struct Orbit
{
    float yawDegrees = 0.0f;   // around Z; 0 looks from +X, 90 from +Y
    float pitchDegrees = 0.0f; // above (+) or below (-) the point
    float zoom = 1.0f;         // 1 = the whole subject fills the picture; 2 = twice as close
};

constexpr float MIN_PITCH_DEGREES = -85.0f;
constexpr float MAX_PITCH_DEGREES = 85.0f;
constexpr float MIN_ZOOM = 0.35f;
constexpr float MAX_ZOOM = 6.0f;

// The orbit after a mouse drag of (dx, dy) pixels: right turns the subject to the
// right (the camera goes left), down tilts the camera up.
Orbit Dragged(const Orbit& orbit, float dxPixels, float dyPixels);
// The orbit after `steps` mouse-wheel notches (positive: closer).
Orbit Zoomed(const Orbit& orbit, float steps);
// The orbit after the automatic turn ran for `seconds`.
Orbit Turned(const Orbit& orbit, float seconds);
// The orbit that looks at the point from `towardCamera` (need not be unit length).
Orbit Facing(const Gizmo::Vec3& towardCamera);

// The angles (degrees, as OBJECT::Angle: AngleMatrix turns X, then Y, then Z) that
// stand a long, thin model upright, its far end (a blade's tip: the side farther
// from the grip at the model origin) at the top, from its bounds at angle zero.
// Zero for a model that is not clearly longer than wide (shields, wings, helms):
// those keep the pose they are made in.
Gizmo::Vec3 UprightAngles(const Gizmo::Vec3& boundsMin, const Gizmo::Vec3& boundsMax);

// A camera ready for the renderer.
struct View
{
    Gizmo::Vec3 eye;
    Gizmo::Vec3 center;
    Gizmo::Vec3 up;
    float fovDegrees = 0.0f; // vertical
    float zNear = 0.0f;
    float zFar = 0.0f;
};

// The camera on `orbit` around `center` that keeps a sphere of `radius` around the
// centre inside a picture of `aspect` (width / height) at zoom 1, from any side.
View OrbitView(const Gizmo::Vec3& center, float radius, const Orbit& orbit, float aspect);

// The world-to-eye transform of `view`: as a column-major 4x4 matrix (what the
// renderer's LoadMatrix takes) and as the engine's 3x4 camera matrix
// (CameraState::Matrix: rows = the camera's right, up and backward axes, last
// column the translation; sprites are placed with it).
void LookAtColumnMajor(const View& view, float out[16]);
void LookAtRows(const View& view, float out[3][4]);

// The side of the square render target for a picture `pixels` wide: rounded up to
// a multiple of TARGET_STEP and kept within [MIN_TARGET, MAX_TARGET], so resizing
// the panel reallocates the target only now and then.
constexpr int TARGET_STEP = 64;
constexpr int MIN_TARGET = 128;
constexpr int MAX_TARGET = 1024;
int TargetSize(float pixels);
} // namespace Editor::Preview

#endif // _EDITOR
