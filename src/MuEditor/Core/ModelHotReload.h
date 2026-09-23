#pragma once

#ifdef _EDITOR

#include "Assets/AssetVariant.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Loads a model (Models[type]) and its textures again from a folder while the
// client runs: the Assets tab switches a model between its current and original
// files (A/B compare) and picks up files that changed on disk.
//
// - Only models in an allowed range are reloaded: the map's world objects always,
//   other blocks of Models[] (items) once an editor allows them with AllowRange.

// - Requests wait for the start of the next frame, before the scene and the ImGui
//   frame are built: then no draw of the frame being recorded and no ImGui draw
//   list can still use a texture that the reload frees.
// - Every request is checked first (Editor::Assets ModelPreflight: the file, its
//   BMD header, mesh, vertex and bone limits, each texture file and its size) and
//   refused with a message, instead of reaching the loaders' fatal paths.
// - A texture keeps its bitmap index when the model had a texture of the same
//   name, so the models that share it show the new image too.
namespace Editor::Assets::HotReload
{
// A block of Models[] the reload may replace, and how the game loads its textures.
struct ModelRange
{
    int first = 0;                  // first model type
    int end = 0;                    // one past the last
    std::string what;               // for messages: "world-object"
    unsigned int textureFilter = 0; // the filter and wrap mode the game's loader uses
    unsigned int textureWrap = 0;
    // Loaded with each map: a request made on another map is dropped. Models the
    // client loads once at start (items) survive a map change.
    bool followsMap = true;
};

// The map's world objects, MODEL_WORLD_OBJECT .. MAX_WORLD_OBJECTS - 1; always allowed.
ModelRange WorldObjectRange();

// Allows reloads of the models in `range` as well. False (and nothing changes)
// when it is empty or overlaps a range allowed before, unless it is exactly that range.
bool AllowRange(const ModelRange& range);

struct Request
{
    int type = -1;         // model type in an allowed range
    std::string modelName; // for messages
    AssetVariant variant = AssetVariant::Current;
    std::filesystem::path bmdFile; // absolute; the textures are read from the same folder
};

struct Outcome
{
    Request request;
    bool loaded = false;
    std::string message; // what was loaded, or why nothing changed
};

// True when `type` is in an allowed range and loaded (for world objects: by this map).
bool CanReload(int type);

// Queues `request`; a waiting request for the same type is replaced.
void Queue(const Request& request);

// Requests still waiting.
std::size_t PendingCount();

// Runs waiting requests, a few per frame. The editor core calls it at the start
// of each frame. Requests for world objects queued on another map are dropped.
void RunPending();

// Results since the last call, oldest first.
std::vector<Outcome> TakeOutcomes();

// The variant the last reload of `type` loaded, while the engine still shows that
// load; empty when the model is as the map loaded it (or the map loaded it again).
std::optional<AssetVariant> LoadedVariant(int type);

// How many of the model's textures the engine reads from under `folder`, out of
// all its textures. A texture shared with another model follows the model that
// was reloaded last.
struct TextureOrigins
{
    int fromFolder = 0;
    int total = 0;
};
TextureOrigins CountTexturesFrom(int type, const std::filesystem::path& folder);
} // namespace Editor::Assets::HotReload

#endif // _EDITOR
