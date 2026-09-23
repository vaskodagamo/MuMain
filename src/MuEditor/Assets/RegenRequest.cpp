#include "RegenRequest.h"

#ifdef _EDITOR

#include "RequestNaming.h"

#include <json.hpp>

#include <algorithm>
#include <map>
#include <set>

using nlohmann::ordered_json;

namespace Editor::Assets
{
namespace
{
constexpr const char* SCHEMA = "mu-regen-request/1";
constexpr const char* AUTHOR = "owner (world editor)";
constexpr const char* FILED_BY = "world editor";
constexpr const char* STATUS_OPEN = "open";
constexpr const char* CAPTURE_VARIANT = "current";
constexpr const char* FORK_REPOSITORY = "vaskodagamo/MuMain";
constexpr const char* BRANCH_PREFIX = "codex/";
constexpr const char* WORKTREE_PREFIX = "../MuMain-";
constexpr const char* REQUEST_BRANCH_WORD = "-req-";
constexpr const char* DELIVERY_FOLDER = "/delivery/";
constexpr const char* CAPTURES_FOLDER = "/captures/";
constexpr std::size_t ID_DATE_PREFIX_CHARS = 11; // "YYYY-MM-DD-"
constexpr int JSON_INDENT = 2;

// Paths a worker must never change, beyond the ones outside its scope anyway.
// The World folders are filled in per world; see "How the editor fills a request".
std::vector<std::string> ProtectedPaths(int world)
{
    const std::string worldFolder = "World" + std::to_string(world);
    return {
        "src/bin/Data/" + worldFolder + "/",
        "src/source/",
        "src/MuEditor/",
        "src/CMakeLists.txt",
        "CMakeLists.txt",
        "CMakePresets.json",
        "cmake/",
        "assets-work/" + worldFolder + "/coordination/",
        "assets-work/" + worldFolder + "/catalog.json",
        "docs/agents/HANDOFF.md",
    };
}

} // namespace

const std::vector<std::string>& MustKeepItems()
{
    static const std::vector<std::string> items = {
        "File names and paths: no renamed, added or deleted game files",
        "Bone count, order, names and parents",
        "Action list: action count, key counts and lock flags",
        "Mesh count and mesh/material order (bmdconv info texture= lines)",
        "Engine-indexed mesh slots (BlendMesh, hidden meshes) and the UV meaning of scrolled meshes",
        "Bind-pose bounds, footprint, pivot and attachment points within a few percent",
        "Textures: power-of-two sizes up to 1024, .jpg opaque, 32-bit .tga for alpha, no new _R/_H/_S/_N name "
        "flags",
    };
    return items;
}

namespace
{
std::string RequestName(const std::string& id)
{
    return id.size() > ID_DATE_PREFIX_CHARS ? id.substr(ID_DATE_PREFIX_CHARS) : id;
}

// "lorencia" in codex/lorencia-req-<model>-<slug>.
std::string WorldWord(const RequestDraft& draft)
{
    return ToLower(draft.worldName);
}

ordered_json OptionalJson(const std::optional<std::string>& value)
{
    return value ? ordered_json(*value) : ordered_json(nullptr);
}

ordered_json PickedJson(const PickedInstance& instance)
{
    ordered_json json;
    json["position"] = instance.position;
    json["rotation"] = instance.rotation;
    json["scale"] = instance.scale;
    json["tile"] = instance.tile;
    if (instance.objIndex)
        json["obj_index"] = *instance.objIndex;
    return json;
}

ordered_json TargetJson(const RequestTarget& target)
{
    const CatalogModel& model = target.model;
    ordered_json textures = ordered_json::object();
    for (const TextureLink& texture : model.textures)
        textures[texture.name] = texture.container;
    ordered_json picked = ordered_json::array();
    for (const PickedInstance& instance : target.picked)
        picked.push_back(PickedJson(instance));

    ordered_json json;
    json["model"] = model.name;
    json["type"] = model.type;
    json["bmd"] = model.bmd;
    json["current_sha256"] = target.sha256;
    json["textures"] = textures;
    json["placement_count"] = model.placementCount;
    json["picked_instances"] = picked;
    json["last_batch"] = OptionalJson(model.lastBatch);
    json["model_dir"] = OptionalJson(model.modelDir);
    json["original"] = {{"revision", model.original.revision}, {"archive", OptionalJson(model.original.archive)}};
    return json;
}

ordered_json ChangeJson(const RequestDraft& draft)
{
    ordered_json json;
    json["summary"] = draft.input.summary;
    json["details"] = draft.input.details;
    json["keep"] = draft.input.keep;
    json["avoid"] = draft.input.avoid;
    if (draft.newModel)
        json["new_model"] = *draft.newModel;
    return json;
}

ordered_json EngineControlJson(const std::string& modelName, const EngineControl& control)
{
    ordered_json json;
    json["model"] = modelName;
    json["source_row"] = control.sourceRow;
    json["controls"] = control.controls;
    json["requirement"] = control.requirement;
    if (control.blendMesh)
        json["blend_mesh"] = *control.blendMesh;
    if (control.blendMeshTexture)
        json["blend_mesh_texture"] = *control.blendMeshTexture;
    return json;
}

ordered_json ConstraintsJson(const RequestDraft& draft)
{
    const RequestScope scope = ComputeScope(draft.input.kind, draft.targets);
    ordered_json shared = ordered_json::object();
    for (const auto& [container, consumers] : scope.sharedTextures)
        shared[container] = consumers;
    ordered_json controls = ordered_json::array();
    for (const RequestTarget& target : draft.targets)
    {
        for (const EngineControl& control : target.model.engineControls)
            controls.push_back(EngineControlJson(target.model.name, control));
    }

    ordered_json json;
    json["shared_textures"] = shared;
    json["frozen_textures"] = scope.frozenTextures;
    json["owned_files"] = scope.ownedFiles;
    json["protected_paths"] = ProtectedPaths(draft.world);
    json["engine_controls"] = controls;
    json["must_keep"] = MustKeepItems();
    return json;
}

ordered_json CaptureJson(const RequestDraft& draft, const CaptureInfo& capture)
{
    ordered_json camera;
    camera["position"] = capture.cameraPosition;
    camera["angle"] = capture.cameraAngle;
    if (capture.cameraDistance)
        camera["distance"] = *capture.cameraDistance;
    camera["free_fly"] = capture.freeFly;

    ordered_json json;
    json["file"] = CapturePath(draft.world, draft.id, capture.fileName);
    json["variant"] = CAPTURE_VARIANT;
    json["camera"] = camera;
    json["hero_tile"] = capture.heroTile ? ordered_json(*capture.heroTile) : ordered_json(nullptr);
    json["resolution"] = {capture.width, capture.height};
    json["client_commit"] = capture.clientCommit.empty() ? ordered_json(nullptr) : ordered_json(capture.clientCommit);
    if (!capture.note.empty())
        json["note"] = capture.note;
    return json;
}

ordered_json EvidenceJson(const RequestDraft& draft)
{
    ordered_json captures = ordered_json::array();
    for (const CaptureInfo& capture : draft.captures)
        captures.push_back(CaptureJson(draft, capture));
    ordered_json previews = ordered_json::array();
    for (const RequestTarget& target : draft.targets)
    {
        for (const std::string* preview : {&target.model.baselinePreview, &target.model.finalPreview})
        {
            if (!preview->empty())
                previews.push_back(*preview);
        }
    }
    return {{"captures", captures}, {"offline_previews", previews}};
}

ordered_json HandoffJson(const RequestDraft& draft)
{
    const std::string name = WorldWord(draft) + REQUEST_BRANCH_WORD + RequestName(draft.id);
    ordered_json json;
    json["repo"] = FORK_REPOSITORY;
    json["branch"] = BRANCH_PREFIX + name;
    json["worktree"] = WORKTREE_PREFIX + name;
    json["deliver_to"] = RequestFolderPath(draft.world, draft.id) + DELIVERY_FOLDER;
    json["push_allowed"] = draft.input.pushAllowed;
    json["claimed_by"] = nullptr;
    json["claimed_at"] = nullptr;
    json["start_commit"] = nullptr;
    return json;
}
} // namespace

const char* KindName(RequestKind kind)
{
    switch (kind)
    {
    case RequestKind::Repaint:
        return "repaint";
    case RequestKind::Remodel:
        return "remodel";
    case RequestKind::RepaintRemodel:
        return "repaint+remodel";
    case RequestKind::NewVariant:
        return "new-variant";
    }
    return "repaint";
}

const char* PriorityName(RequestPriority priority)
{
    switch (priority)
    {
    case RequestPriority::Low:
        return "low";
    case RequestPriority::Normal:
        return "normal";
    case RequestPriority::High:
        return "high";
    }
    return "normal";
}

std::string RequestFolderPath(int world, const std::string& id)
{
    return "assets-work/World" + std::to_string(world) + "/requests/" + id;
}

std::string CapturePath(int world, const std::string& id, const std::string& fileName)
{
    return RequestFolderPath(world, id) + CAPTURES_FOLDER + fileName;
}

RequestScope ComputeScope(RequestKind kind, const std::vector<RequestTarget>& targets)
{
    std::set<std::string> targetNames;
    std::map<std::string, std::set<std::string>> consumers;
    std::set<std::string> bmds;
    for (const RequestTarget& target : targets)
    {
        targetNames.insert(target.model.name);
        bmds.insert(target.model.bmd);
        for (const auto& [container, users] : TextureConsumers(target.model))
            consumers[container].insert(users.begin(), users.end());
    }

    RequestScope scope;
    std::set<std::string> owned;
    for (const auto& [container, users] : consumers)
    {
        if (users.size() > 1)
            scope.sharedTextures.emplace_back(container, std::vector<std::string>(users.begin(), users.end()));
        const bool usedOutside = std::any_of(users.begin(), users.end(),
                                             [&](const std::string& user) { return !targetNames.contains(user); });
        if (usedOutside)
            scope.frozenTextures.push_back(container);
        else if (kind == RequestKind::Repaint || kind == RequestKind::RepaintRemodel)
            owned.insert(container);
    }
    if (kind == RequestKind::Remodel || kind == RequestKind::RepaintRemodel)
        owned.insert(bmds.begin(), bmds.end());
    scope.ownedFiles.assign(owned.begin(), owned.end());
    return scope;
}

std::string BuildRequestJson(const RequestDraft& draft)
{
    ordered_json history = ordered_json::array();
    history.push_back({{"status", STATUS_OPEN}, {"at", draft.created}, {"by", FILED_BY}});
    ordered_json targets = ordered_json::array();
    for (const RequestTarget& target : draft.targets)
        targets.push_back(TargetJson(target));

    ordered_json json;
    json["schema"] = SCHEMA;
    json["id"] = draft.id;
    json["created"] = draft.created;
    json["author"] = AUTHOR;
    json["world"] = draft.world;
    json["status"] = STATUS_OPEN;
    json["status_history"] = history;
    json["priority"] = PriorityName(draft.input.priority);
    json["kind"] = KindName(draft.input.kind);
    json["base_commit"] = draft.baseCommit;
    json["supersedes"] = nullptr;
    json["targets"] = targets;
    json["change"] = ChangeJson(draft);
    json["constraints"] = ConstraintsJson(draft);
    json["evidence"] = EvidenceJson(draft);
    json["handoff"] = HandoffJson(draft);
    json["result"] = nullptr;
    json["decision"] = nullptr;
    // Invalid UTF-8 in typed text becomes U+FFFD instead of failing the request.
    return json.dump(JSON_INDENT, ' ', false, ordered_json::error_handler_t::replace) + "\n";
}
} // namespace Editor::Assets

#endif // _EDITOR
