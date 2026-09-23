#pragma once

#ifdef _EDITOR

#include "AssetCatalog.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

// A regeneration request (schema "mu-regen-request/1"): the owner asks the art
// builder to rework models that are already in the game. The contract, every
// field and who may change it are in assets-work/World1/requests/README.md.
// This file builds request.json from the catalog facts and the owner's input
// (RequestBrief.h writes the matching brief.md); nothing here touches the disk
// or the engine.
namespace Editor::Assets
{
enum class RequestKind
{
    Repaint,        // new textures, same meshes
    Remodel,        // new BMD, same textures
    RepaintRemodel, // both
    NewVariant,     // an additional model derived from one target; installs nothing
};

enum class RequestPriority
{
    Low,
    Normal,
    High,
};

const char* KindName(RequestKind kind);
const char* PriorityName(RequestPriority priority);

// What the owner typed into the "Flag for regeneration" dialog.
struct OwnerInput
{
    RequestKind kind = RequestKind::Repaint;
    RequestPriority priority = RequestPriority::Normal;
    std::string summary;
    std::vector<std::string> details;
    std::vector<std::string> keep;
    std::vector<std::string> avoid;
    bool pushAllowed = false; // the owner allows the worker to push a branch and open a PR
};

// A placed instance the owner picked in the world.
struct PickedInstance
{
    std::array<float, 3> position{};
    std::array<float, 3> rotation{}; // degrees
    float scale = 1.0f;
    std::array<double, 2> tile{}; // position in terrain tiles (position / 100, as the catalog has it)
    std::optional<int> objIndex;  // record index in EncTerrain{N}.obj, when it matches one exactly
};

// One model the request asks to change.
struct RequestTarget
{
    CatalogModel model;
    std::string sha256; // of the BMD at base_commit
    std::vector<PickedInstance> picked;
};

// The in-client screenshot attached as evidence.
struct CaptureInfo
{
    std::string fileName = "01-current.jpg"; // inside the request's captures/ folder
    std::array<float, 3> cameraPosition{};
    std::array<float, 3> cameraAngle{};
    std::optional<float> cameraDistance; // the game camera's distance; none for free-fly
    bool freeFly = false;
    std::optional<std::array<float, 2>> heroTile; // none offline (no character)
    int width = 0;
    int height = 0;
    std::string clientCommit; // short sha, empty when unknown
    std::string note;
};

struct RequestDraft
{
    std::string id; // <date>-<model>-<slug>, also the folder name
    std::string created;
    int world = 1;
    std::string worldName = "Lorencia"; // names the worker branch: codex/lorencia-req-...
    std::string baseCommit;             // full sha
    OwnerInput input;
    std::vector<RequestTarget> targets; // the clicked model first
    std::optional<std::string> newModel;
    std::vector<CaptureInfo> captures;
};

// Repo-relative paths derived from the id.
std::string RequestFolderPath(int world, const std::string& id); // assets-work/World1/requests/<id>
std::string CapturePath(int world, const std::string& id, const std::string& fileName);

// Containers of the targets -> every model that uses them (only the ones used by
// more than one model), frozen containers (a consumer outside the targets) and the
// game files the worker may replace for the request's kind.
struct RequestScope
{
    std::vector<std::pair<std::string, std::vector<std::string>>> sharedTextures;
    std::vector<std::string> frozenTextures;
    std::vector<std::string> ownedFiles;
};
RequestScope ComputeScope(RequestKind kind, const std::vector<RequestTarget>& targets);

// The fixed constraints.must_keep list of the README's example: what a rebuilt
// model keeps so the engine and every placement still work.
const std::vector<std::string>& MustKeepItems();

// request.json text, fields in the README's order, two-space indent.
std::string BuildRequestJson(const RequestDraft& draft);
} // namespace Editor::Assets

#endif // _EDITOR
