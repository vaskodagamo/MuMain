#pragma once

#ifdef _EDITOR

#include "Render/Models/ZzzBMD.h" // BMD, vec3_t

// Facts about a loaded model (Models[]) in the pose the engine's BoneTransform
// holds after BMD::Animation, for the editor's thumbnails and item preview.
namespace Editor::ModelPose
{
// The model's bounds in that pose, in model units before BodyScale and
// BodyOrigin, from its vertices (BMD::Transform does not return them). False for
// a model without vertices.
bool Bounds(const BMD& model, vec3_t boundsMin, vec3_t boundsMax);
} // namespace Editor::ModelPose

#endif // _EDITOR
