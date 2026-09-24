#include "ScriptParser.h"

#ifdef _EDITOR

#include "ObjectOpParser.h"
#include "ScriptReader.h"
#include "SurfaceOpParser.h"

namespace Editor::MapScript
{
namespace
{
using nlohmann::json;

bool IsTerrain(OpKind kind)
{
    return kind >= OpKind::TerrainRaise && kind <= OpKind::TerrainNoise;
}

bool IsTexture(OpKind kind)
{
    return kind == OpKind::TexturePaint || kind == OpKind::TextureErase;
}

bool IsLight(OpKind kind)
{
    return kind >= OpKind::LightAdd && kind <= OpKind::LightBake;
}

// Reads the fields of `op.kind` into the matching member of `op.data`.
template <typename Edit, typename ReadFields> bool ReadInto(Op& op, ReadFields readFields)
{
    Edit edit;
    if (!readFields(edit))
        return false;
    op.data = std::move(edit);
    return true;
}

bool OpFields(const ScriptReader& reader, Op& op)
{
    const OpKind kind = op.kind;
    if (IsTerrain(kind))
        return ReadInto<TerrainEdit>(op, [&](TerrainEdit& edit) { return Parse::TerrainOp(reader, kind, edit); });
    if (IsTexture(kind))
        return ReadInto<TextureEdit>(op, [&](TextureEdit& edit) { return Parse::TextureOp(reader, kind, edit); });
    if (kind == OpKind::AttributeSet)
        return ReadInto<AttributeEdit>(op, [&](AttributeEdit& edit) { return Parse::AttributeOp(reader, edit); });
    if (IsLight(kind))
        return ReadInto<LightEdit>(op, [&](LightEdit& edit) { return Parse::LightOp(reader, kind, edit); });
    if (kind == OpKind::ObjectPlace)
        return ReadInto<PlaceEdit>(op, [&](PlaceEdit& edit) { return Parse::PlaceOp(reader, edit); });
    if (kind == OpKind::ObjectScatter)
        return ReadInto<ScatterEdit>(op, [&](ScatterEdit& edit) { return Parse::ScatterOp(reader, edit); });
    return ReadInto<ObjectEdit>(op, [&](ObjectEdit& edit) { return Parse::ObjectEditOp(reader, kind, edit); });
}

bool ReadOp(const ScriptReader& reader, int index, Op& op)
{
    std::string name;
    if (!reader.IsObject())
        return reader.Fail({}, "is an op object such as {\"op\": \"terrain.raise\", ...}");
    if (!reader.Text("op", name))
        return false;
    if (!OpKindFromName(name, op.kind))
        return reader.Fail("op", "\"" + name + "\" is not an op; known: " + OpNames());
    op.index = index;
    return OpFields(reader, op);
}

// The selector of an op (an object edit's "select", attribute.set's "under") and its
// field name; nullptr when the op has none.
const ObjectSelector* SelectorOf(const Op& op, const char*& field)
{
    if (const ObjectEdit* edit = std::get_if<ObjectEdit>(&op.data))
    {
        field = "select";
        return &edit->select;
    }
    const AttributeEdit* attribute = std::get_if<AttributeEdit>(&op.data);
    field = "under";
    return attribute != nullptr && attribute->under ? &*attribute->under : nullptr;
}

// Ops may name only earlier ops in "placed_by", and only ones that place objects.
bool CheckPlacedBy(const EditScript& script, const ScriptReader& root)
{
    for (const Op& op : script.ops)
    {
        const char* field = nullptr;
        const ObjectSelector* selector = SelectorOf(op, field);
        if (selector == nullptr || !selector->placedBy)
            continue;
        const int placer = *selector->placedBy;
        const bool earlier = placer >= 0 && placer < op.index;
        const OpKind kind = earlier ? script.ops[static_cast<std::size_t>(placer)].kind : OpKind::TerrainRaise;
        if (!earlier || (kind != OpKind::ObjectPlace && kind != OpKind::ObjectScatter))
            return root.Element("ops", static_cast<std::size_t>(op.index))
                .Nested(field)
                .Fail("placed_by", "names an earlier object.place or object.scatter op of this script by its index");
    }
    return true;
}

bool ReadHeader(const ScriptReader& root, EditScript& script)
{
    std::string schema;
    if (!root.OnlyKeys({"schema", "label", "ops"}) || !root.Text("schema", schema))
        return false;
    if (schema != SCRIPT_SCHEMA)
        return root.Fail("schema", "is \"" + std::string(SCRIPT_SCHEMA) + "\" (this client reads no other version)");
    if (!root.OptionalText("label", script.label))
        return false;
    if (script.label.size() > MAX_LABEL_CHARS)
        return root.Fail("label", "is at most " + std::to_string(MAX_LABEL_CHARS) + " characters");
    const json& ops = root.At("ops");
    if (!ops.is_array() || ops.empty() || ops.size() > MAX_OPS)
        return root.Fail("ops", "is a list of 1 to " + std::to_string(MAX_OPS) + " ops");
    return true;
}
} // namespace

bool ParseScript(const json& document, EditScript& script, std::string& error)
{
    error.clear();
    script = EditScript{};
    const ScriptReader root(document, "", error);
    if (!root.IsObject())
        return root.Fail({}, "an edit script is a JSON object with \"schema\", \"label\" and \"ops\"");
    if (!ReadHeader(root, script))
        return false;
    const json& ops = root.At("ops");
    script.ops.resize(ops.size());
    for (std::size_t i = 0; i < ops.size(); ++i)
    {
        if (!ReadOp(root.Element("ops", i), static_cast<int>(i), script.ops[i]))
            return false;
    }
    return CheckPlacedBy(script, root);
}

bool ParseScript(const std::string& text, EditScript& script, std::string& error)
{
    const json document = json::parse(text, nullptr, false);
    if (document.is_discarded())
    {
        error = "the script is not valid JSON";
        return false;
    }
    return ParseScript(document, script, error);
}
} // namespace Editor::MapScript

#endif // _EDITOR
