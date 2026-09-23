#include "stdafx.h"

#ifdef _EDITOR

#include "ModelPose.h"

#include <cmath>

namespace Editor::ModelPose
{
namespace
{
constexpr float NO_BOUND = 1e9f;
} // namespace

bool Bounds(const BMD& model, vec3_t boundsMin, vec3_t boundsMax)
{
    Vector(NO_BOUND, NO_BOUND, NO_BOUND, boundsMin);
    Vector(-NO_BOUND, -NO_BOUND, -NO_BOUND, boundsMax);
    bool any = false;
    for (int mesh = 0; mesh < model.NumMeshs; ++mesh)
    {
        const Mesh_t& meshData = model.Meshs[mesh];
        for (int vertex = 0; vertex < meshData.NumVertices; ++vertex)
        {
            const Vertex_t& source = meshData.Vertices[vertex];
            vec3_t position;
            VectorTransform(source.Position, BoneTransform[source.Node], position);
            for (int axis = 0; axis < 3; ++axis)
            {
                boundsMin[axis] = std::fmin(boundsMin[axis], position[axis]);
                boundsMax[axis] = std::fmax(boundsMax[axis], position[axis]);
            }
            any = true;
        }
    }
    return any;
}
} // namespace Editor::ModelPose

#endif // _EDITOR
