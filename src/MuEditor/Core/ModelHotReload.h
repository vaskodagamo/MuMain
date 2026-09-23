#pragma once

#ifdef _EDITOR

#include "Assets/AssetVariant.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Loads a world-object model (Models[type]) and its textures again from a folder
// while the client runs: the Assets tab switches a model between its current and
// original files (A/B compare) and picks up files that changed on disk.
//
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
struct Request
{
    int type = -1;         // world-object model type, 0 .. MAX_WORLD_OBJECTS - 1
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

// True when `type` is a world object this map has loaded.
bool CanReload(int type);

// Queues `request`; a waiting request for the same type is replaced.
void Queue(const Request& request);

// Requests still waiting.
std::size_t PendingCount();

// Runs waiting requests, a few per frame. The editor core calls it at the start
// of each frame. Requests queued on another map are dropped.
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
