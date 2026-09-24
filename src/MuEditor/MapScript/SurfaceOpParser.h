#pragma once

#ifdef _EDITOR

#include "EditScript.h"
#include "ScriptReader.h"

// Reading the ops that change the terrain's layers: terrain.*, texture.*,
// attribute.set and light.*.
namespace Editor::MapScript::Parse
{
constexpr int MAX_ITERATIONS = 64;
constexpr int MAX_OCTAVES = 8;
constexpr float MIN_NOISE_SCALE = 0.5f;
constexpr float MAX_NOISE_SCALE = 256.0f;
// light.bake: a sun at least this high (lower throws whole hillsides into black), and
// how strongly slopes may change the light.
constexpr float MIN_SUN_ELEVATION = 5.0f;
constexpr float MAX_SUN_ELEVATION = 90.0f;
constexpr float MIN_RELIEF_CONTRAST = 0.0f;
constexpr float MAX_RELIEF_CONTRAST = 4.0f;

// Defaults an op takes when the script leaves a field out.
constexpr float DEFAULT_NOISE_SCALE = 8.0f;
constexpr int DEFAULT_OCTAVES = 3;
constexpr float DEFAULT_LIGHT_ADD_STRENGTH = 0.2f;
constexpr float DEFAULT_LIGHT_TINT_STRENGTH = 0.5f;

bool TerrainOp(const ScriptReader& reader, OpKind kind, TerrainEdit& edit);
bool TextureOp(const ScriptReader& reader, OpKind kind, TextureEdit& edit);
bool AttributeOp(const ScriptReader& reader, AttributeEdit& edit);
bool LightOp(const ScriptReader& reader, OpKind kind, LightEdit& edit);
} // namespace Editor::MapScript::Parse

#endif // _EDITOR
