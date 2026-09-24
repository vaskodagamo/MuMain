#include "EditScript.h"

#ifdef _EDITOR

#include <algorithm>
#include <array>

namespace Editor::MapScript
{
namespace
{
struct NamedOp
{
    OpKind kind;
    const char* name;
};

constexpr std::array<NamedOp, 23> OP_NAMES = {{
    {OpKind::TerrainRaise, "terrain.raise"},
    {OpKind::TerrainLower, "terrain.lower"},
    {OpKind::TerrainFlatten, "terrain.flatten"},
    {OpKind::TerrainSet, "terrain.set"},
    {OpKind::TerrainRamp, "terrain.ramp"},
    {OpKind::TerrainSmooth, "terrain.smooth"},
    {OpKind::TerrainNoise, "terrain.noise"},
    {OpKind::TexturePaint, "texture.paint"},
    {OpKind::TextureErase, "texture.erase"},
    {OpKind::AttributeSet, "attribute.set"},
    {OpKind::LightAdd, "light.add"},
    {OpKind::LightSubtract, "light.subtract"},
    {OpKind::LightTint, "light.tint"},
    {OpKind::LightSet, "light.set"},
    {OpKind::LightSmooth, "light.smooth"},
    {OpKind::LightBake, "light.bake"},
    {OpKind::ObjectPlace, "object.place"},
    {OpKind::ObjectScatter, "object.scatter"},
    {OpKind::ObjectMove, "object.move"},
    {OpKind::ObjectRotate, "object.rotate"},
    {OpKind::ObjectScale, "object.scale"},
    {OpKind::ObjectDelete, "object.delete"},
    {OpKind::ObjectDropToGround, "object.drop_to_ground"},
}};

constexpr const char* SUMMARY_PREFIX = "Script: ";
// A summary names this many distinct ops, then says how many more there are.
constexpr std::size_t SUMMARY_NAMES = 3;
} // namespace

const char* OpName(OpKind kind)
{
    for (const NamedOp& op : OP_NAMES)
    {
        if (op.kind == kind)
            return op.name;
    }
    return "unknown";
}

bool OpKindFromName(const std::string& name, OpKind& kind)
{
    for (const NamedOp& op : OP_NAMES)
    {
        if (name == op.name)
        {
            kind = op.kind;
            return true;
        }
    }
    return false;
}

std::string OpNames()
{
    std::string names;
    for (const NamedOp& op : OP_NAMES)
    {
        if (!names.empty())
            names += ", ";
        names += op.name;
    }
    return names;
}

std::string OpLabel(const Op& op)
{
    return "ops[" + std::to_string(op.index) + "] (" + OpName(op.kind) + ")";
}

std::string OpField(const Op& op, const std::string& field)
{
    return "ops[" + std::to_string(op.index) + "]." + field;
}

std::string StepLabel(const EditScript& script)
{
    if (!script.label.empty())
        return script.label;
    std::vector<std::string> distinct;
    for (const Op& op : script.ops)
    {
        const std::string name = OpName(op.kind);
        if (std::find(distinct.begin(), distinct.end(), name) == distinct.end())
            distinct.push_back(name);
    }
    std::string label = SUMMARY_PREFIX;
    for (std::size_t i = 0; i < distinct.size() && i < SUMMARY_NAMES; ++i)
        label += (i == 0 ? "" : ", ") + distinct[i];
    if (distinct.size() > SUMMARY_NAMES)
        label += " and " + std::to_string(distinct.size() - SUMMARY_NAMES) + " more";
    return label;
}
} // namespace Editor::MapScript

#endif // _EDITOR
