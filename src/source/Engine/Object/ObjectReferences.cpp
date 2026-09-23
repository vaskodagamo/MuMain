#include "stdafx.h"

#include "ObjectReferences.h"

#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"

// Index of the operate under the mouse cursor, or -1 (ZzzInterface.cpp).
extern int SelectedOperate;

namespace Engine::Object
{
namespace
{
constexpr int NO_OPERATE = -1;

template <typename IsReleased> void ReleaseOperates(IsReleased isReleased)
{
    for (int i = 0; i < MAX_OPERATES; ++i)
    {
        OPERATE& operate = Operates[i];
        if (operate.Owner == nullptr || !isReleased(operate.Owner))
            continue;

        operate.Live = false;
        operate.Owner = nullptr;
        if (SelectedOperate == i)
            SelectedOperate = NO_OPERATE;
    }
}

template <typename IsReleased> void EndAttachedEffects(IsReleased isReleased)
{
    for (int i = 0; i < MAX_EFFECTS; ++i)
    {
        OBJECT& effect = Effects[i];
        if (effect.Live && effect.Owner != nullptr && isReleased(effect.Owner))
        {
            effect.Live = false;
            effect.Owner = nullptr;
        }
    }
    for (int i = 0; i < MAX_JOINTS; ++i)
    {
        JOINT& joint = Joints[i];
        if (joint.Live && joint.Target != nullptr && isReleased(joint.Target))
        {
            joint.Live = false;
            joint.Target = nullptr;
        }
    }
    for (int i = 0; i < MAX_PARTICLES; ++i)
    {
        PARTICLE& particle = Particles[i];
        if (particle.Live && particle.Target != nullptr && isReleased(particle.Target))
        {
            particle.Live = false;
            particle.Target = nullptr;
        }
    }
}

template <typename IsReleased> void ReleaseMatching(IsReleased isReleased)
{
    ReleaseOperates(isReleased);
    EndAttachedEffects(isReleased);
}
} // namespace

void ReleaseReferencesTo(const OBJECT* object)
{
    if (object == nullptr)
        return;
    ReleaseMatching([object](const OBJECT* referenced) { return referenced == object; });
}

void ReleaseReferencesTo(const std::unordered_set<const OBJECT*>& objects)
{
    if (objects.empty())
        return;
    ReleaseMatching([&objects](const OBJECT* referenced) { return objects.count(referenced) != 0; });
}
} // namespace Engine::Object
