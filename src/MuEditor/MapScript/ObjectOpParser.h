#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "ScriptReader.h"

#include "Editing/ObjectTransform.h" // MIN_OBJECT_SCALE

// Reading the object ops: object.place, object.scatter and the edits of selected objects.
namespace Editor::MapScript::Parse
{
// The smallest scale a script gives: the one every object edit keeps (Objects tab, gizmo).
constexpr float MIN_OBJECT_SCALE = Editor::Editing::Transform::MIN_OBJECT_SCALE;
constexpr float MAX_OBJECT_SCALE = 20.0f;
constexpr float ANGLE_LIMIT = 3600.0f;
constexpr float MAX_DENSITY = 4.0f; // objects per tile
constexpr float MAX_SPACING = 64.0f;
constexpr std::size_t MAX_MODEL_CHOICES = 32;
constexpr std::size_t MAX_SELECTOR_IDS = 32767;
constexpr std::size_t MAX_AVOID_ENTRIES = 64;

// Defaults.
constexpr float DEFAULT_MIN_SPACING = 1.0f;
constexpr float FULL_TURN = 360.0f;

bool PlaceOp(const ScriptReader& reader, PlaceEdit& edit);
// The object selector in field `key` ({"ids", "model", "models", "inside", "placed_by"}, at
// least one of them): the object edits' "select", attribute.set's "under".
bool SelectorAt(const ScriptReader& reader, const char* key, ObjectSelector& selector);
bool ScatterOp(const ScriptReader& reader, ScatterEdit& edit);
bool ObjectEditOp(const ScriptReader& reader, OpKind kind, ObjectEdit& edit);
} // namespace Editor::MapScript::Parse

#endif // _EDITOR
