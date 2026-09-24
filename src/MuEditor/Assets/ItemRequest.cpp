#include "ItemRequest.h"

#ifdef _EDITOR

#include <json.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <set>
#include <tuple>

using nlohmann::ordered_json;

namespace Editor::Assets
{
namespace
{
constexpr const char* STATUS_OPEN = "open";
constexpr const char* CAPTURE_VARIANT = "current";
constexpr const char* OTHER_CONSUMER_PREFIX = "other:";
constexpr int FIRST_ARMOUR_GROUP = 7;
constexpr int LAST_ARMOUR_GROUP = 11;
constexpr int MAX_TRIANGLES = 1500;
constexpr int MAX_TEXTURE_SIZE = 1024;
constexpr int JSON_INDENT = 2;

// Only these folders hold files a worker may replace; a texture elsewhere is frozen.
constexpr const char* OWNABLE_FOLDERS[] = {"src/bin/Data/Item/", "src/bin/Data/Player/"};

// The catalog's class keys in RequireClass order (DW, DK, Elf, MG, DL, SUM, RF).
constexpr const char* CLASS_KEYS[ITEM_CLASS_COUNT] = {"dw", "dk", "elf", "mg", "dl", "sum", "rf"};

// In ItemRequestKind order.
constexpr const char* KIND_NAMES[ITEM_REQUEST_KIND_COUNT] = {"upscale", "repaint", "remodel", "redesign", "set"};

// assets-work/Items/requests/README.md, "What a rebuilt item keeps"; the validator checks them verbatim.
const std::vector<std::string>& CommonMustKeep()
{
    static const std::vector<std::string> lines = {
        "File names and paths: no renamed, added or deleted game files",
        "Origin, orientation and scale: hands, back and shields attach through the model origin "
        "(RenderLinkObject angles are hard-coded)",
        "Mesh count and mesh/material order (the engine hides and blends meshes by index)",
        "Texture name suffixes _R, _S, _H, _N (they set render flags); no new ones",
        "At most 1500 triangles per model; power-of-two textures up to 1024 px, .jpg opaque, 32-bit .tga for alpha",
        "Armour and wings: the skeleton (bone count, order, names, parents) and every action with its key count",
        "Size in the inventory (Width x Height of the item table) and a footprint that fits it",
    };
    return lines;
}

// In ItemRequestKind order.
const std::array<const char*, ITEM_REQUEST_KIND_COUNT> KIND_MUST_KEEP = {
    "Upscale: the design, the mesh and the UV layout (unless the request says otherwise), the texture name suffixes",
    "Repaint: the mesh and the UV layout",
    "Remodel: the silhouette and the function; origin, orientation, scale and mesh order",
    "Redesign: origin, grip, size in the inventory slot and mesh order",
    "Set: every part keeps its skeleton and actions, and the parts still read as one set",
};

bool IsOwnable(const std::string& path)
{
    return std::any_of(std::begin(OWNABLE_FOLDERS), std::end(OWNABLE_FOLDERS),
                       [&](const char* folder) { return path.starts_with(folder); });
}

bool ReplacesModels(ItemRequestKind partKind)
{
    return partKind == ItemRequestKind::Remodel || partKind == ItemRequestKind::Redesign;
}

// Item keys by group and index, then other:<MODEL_...> by name (the catalog's order).
auto ConsumerOrder(const std::string& consumer)
{
    if (consumer.starts_with(OTHER_CONSUMER_PREFIX))
        return std::make_tuple(1, 0, 0, consumer);
    const std::size_t dash = consumer.find('-');
    const int group = std::atoi(consumer.substr(0, dash).c_str());
    const int index = dash == std::string::npos ? 0 : std::atoi(consumer.substr(dash + 1).c_str());
    return std::make_tuple(0, group, index, std::string());
}

std::vector<std::string> SortedConsumers(const std::set<std::string>& consumers)
{
    std::vector<std::string> sorted(consumers.begin(), consumers.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const std::string& a, const std::string& b) { return ConsumerOrder(a) < ConsumerOrder(b); });
    return sorted;
}

ordered_json NameOrNull(const std::string& text)
{
    return text.empty() ? ordered_json(nullptr) : ordered_json(text);
}

ordered_json ModelJson(const ItemModel& model, const std::string& sha256)
{
    ordered_json textures = ordered_json::object();
    for (const ItemTexture& texture : model.textures)
        textures[texture.name] = NameOrNull(texture.container);

    ordered_json json;
    json["role"] = model.role;
    if (!model.className.empty())
        json["class"] = model.className;
    json["bmd"] = model.bmd;
    json["current_sha256"] = sha256;
    json["textures"] = textures;
    return json;
}

ordered_json TargetJson(const ItemRequestTarget& target)
{
    const ItemCatalogEntry& item = target.item;
    ordered_json classes = ordered_json::object();
    for (int i = 0; i < ITEM_CLASS_COUNT; ++i)
        classes[CLASS_KEYS[i]] = item.classStages[i];
    ordered_json models = ordered_json::array();
    for (std::size_t i = 0; i < item.models.size(); ++i)
        models.push_back(ModelJson(item.models[i], i < target.modelSha256.size() ? target.modelSha256[i] : ""));

    ordered_json json;
    json["key"] = item.key;
    json["group"] = item.group;
    json["index"] = item.index;
    json["name"] = NameOrNull(item.name);
    json["family"] = item.family;
    json["tier"] = item.tier.value;
    json["classes"] = classes;
    json["size"] = {item.width, item.height};
    json["armour_set"] = item.armourSet ? ordered_json(*item.armourSet) : ordered_json(nullptr);
    json["models"] = models;
    json["original"] = {{"revision", NameOrNull(item.originalRevision)}, {"sha256", item.originalSha256}};
    return json;
}

ordered_json ChangeJson(const ItemRequestDraft& draft)
{
    const ItemOwnerInput& input = draft.input;
    ordered_json json;
    json["summary"] = input.summary;
    json["details"] = input.details;
    json["keep"] = input.keep;
    json["avoid"] = input.avoid;
    if (input.kind == ItemRequestKind::Set)
        json["set_kind"] = ItemKindName(input.setKind);
    if (draft.referenceImages.empty())
        return json;
    ordered_json references = ordered_json::array();
    for (const std::string& fileName : draft.referenceImages)
        references.push_back(CapturePath(draft.domain, draft.id, fileName));
    json["reference_images"] = references;
    return json;
}

ordered_json DrawModeJson(const std::string& mode)
{
    return NameOrNull(mode); // empty: the model is not used in that context
}

ordered_json RenderModelJson(const ItemModelDraw& model)
{
    ordered_json meshes = ordered_json::array();
    for (const ItemMeshDraw& mesh : model.meshes)
    {
        ordered_json json;
        json["mesh"] = mesh.mesh;
        json["texture"] = mesh.texture;
        json["worn"] = DrawModeJson(mesh.worn);
        json["dropped"] = DrawModeJson(mesh.dropped);
        json["inventory"] = DrawModeJson(mesh.inventory);
        meshes.push_back(json);
    }
    ordered_json json;
    json["role"] = model.role;
    json["bmd"] = model.bmd;
    json["meshes"] = meshes;
    return json;
}

// constraints.render: per target, how the game draws its meshes and what it adds.
ordered_json RenderJson(const std::vector<ItemRequestTarget>& targets)
{
    ordered_json render = ordered_json::object();
    for (const ItemRequestTarget& target : targets)
    {
        ordered_json models = ordered_json::array();
        ordered_json effects = ordered_json::array();
        if (target.render)
        {
            for (const ItemModelDraw& model : target.render->models)
                models.push_back(RenderModelJson(model));
            for (const std::string& effect : target.render->effects)
                effects.push_back(effect);
        }
        render[target.item.key] = {{"models", models}, {"effects", effects}};
    }
    return render;
}

ordered_json ConstraintsJson(const ItemRequestDraft& draft)
{
    const ItemRequestScope scope = ComputeItemScope(PartKind(draft.input), draft.targets);
    ordered_json shared = ordered_json::object();
    for (const auto& [container, consumers] : scope.sharedTextures)
        shared[container] = consumers;

    ordered_json json;
    json["shared_textures"] = shared;
    json["frozen_textures"] = scope.frozenTextures;
    json["owned_files"] = scope.ownedFiles;
    json["protected_paths"] = draft.domain.protectedPaths;
    json["limits"] = {{"max_triangles", MAX_TRIANGLES}, {"max_texture_size", MAX_TEXTURE_SIZE}};
    json["must_keep"] = ItemRequestMustKeep(draft);
    json["render"] = RenderJson(draft.targets);
    return json;
}

ordered_json CaptureJson(const ItemRequestDraft& draft, const ItemCaptureInfo& capture)
{
    ordered_json json;
    json["file"] = CapturePath(draft.domain, draft.id, capture.fileName);
    json["variant"] = CAPTURE_VARIANT;
    json["view"] = capture.view;
    if (capture.angle)
        json["angle"] = *capture.angle;
    json["item_level"] = capture.itemLevel;
    json["excellent"] = capture.excellent;
    json["ancient"] = capture.ancient;
    json["resolution"] = {capture.width, capture.height};
    json["client_commit"] = NameOrNull(capture.clientCommit);
    if (!capture.note.empty())
        json["note"] = capture.note;
    return json;
}

ordered_json EvidenceJson(const ItemRequestDraft& draft)
{
    ordered_json captures = ordered_json::array();
    for (const ItemCaptureInfo& capture : draft.captures)
        captures.push_back(CaptureJson(draft, capture));
    return {{"captures", captures}, {"offline_previews", ordered_json::array()}};
}

ordered_json HandoffJson(const ItemRequestDraft& draft)
{
    ordered_json json;
    json["repo"] = REQUEST_REPOSITORY;
    json["branch"] = RequestBranch(draft.domain, draft.id);
    json["worktree"] = RequestWorktree(draft.domain, draft.id);
    json["deliver_to"] = RequestDeliveryPath(draft.domain, draft.id);
    json["push_allowed"] = draft.input.pushAllowed;
    json["claimed_by"] = nullptr;
    json["claimed_at"] = nullptr;
    json["start_commit"] = nullptr;
    return json;
}

// Container -> the targets using it, and its consumers outside the targets.
struct ContainerUse
{
    std::set<std::string> consumers;
    bool usedOutside = false;
};

std::map<std::string, ContainerUse> ContainerUses(const std::vector<ItemRequestTarget>& targets)
{
    std::set<std::string> keys;
    for (const ItemRequestTarget& target : targets)
        keys.insert(target.item.key);
    std::map<std::string, ContainerUse> uses;
    for (const ItemRequestTarget& target : targets)
    {
        for (const ItemModel& model : target.item.models)
        {
            for (const ItemTexture& texture : model.textures)
            {
                if (!texture.container.empty())
                    uses[texture.container].consumers.insert(target.item.key);
            }
        }
    }
    for (auto& [container, use] : uses)
    {
        for (const ItemRequestTarget& target : targets)
        {
            const auto shared = target.item.sharedWith.find(container);
            if (shared != target.item.sharedWith.end())
                use.consumers.insert(shared->second.begin(), shared->second.end());
        }
        use.usedOutside = std::any_of(use.consumers.begin(), use.consumers.end(),
                                      [&](const std::string& consumer) { return !keys.contains(consumer); });
    }
    return uses;
}
} // namespace

const char* ItemKindName(ItemRequestKind kind)
{
    const int index = static_cast<int>(kind);
    return index >= 0 && index < ITEM_REQUEST_KIND_COUNT ? KIND_NAMES[index] : KIND_NAMES[0];
}

std::optional<ItemRequestKind> ParseItemKind(const std::string& name)
{
    for (int i = 0; i < ITEM_REQUEST_KIND_COUNT; ++i)
    {
        if (name == KIND_NAMES[i])
            return static_cast<ItemRequestKind>(i);
    }
    return std::nullopt;
}

bool IsArmourGroup(int group)
{
    return group >= FIRST_ARMOUR_GROUP && group <= LAST_ARMOUR_GROUP;
}

ItemRequestKind PartKind(const ItemOwnerInput& input)
{
    return input.kind == ItemRequestKind::Set ? input.setKind : input.kind;
}

ItemRequestScope ComputeItemScope(ItemRequestKind partKind, const std::vector<ItemRequestTarget>& targets)
{
    ItemRequestScope scope;
    std::set<std::string> owned;
    for (const auto& [container, use] : ContainerUses(targets))
    {
        if (use.consumers.size() > 1)
            scope.sharedTextures.emplace_back(container, SortedConsumers(use.consumers));
        if (use.usedOutside || !IsOwnable(container))
            scope.frozenTextures.push_back(container);
        else
            owned.insert(container);
    }
    if (ReplacesModels(partKind))
    {
        for (const ItemRequestTarget& target : targets)
        {
            for (const ItemModel& model : target.item.models)
            {
                if (model.exists && IsOwnable(model.bmd))
                    owned.insert(model.bmd);
            }
        }
    }
    scope.ownedFiles.assign(owned.begin(), owned.end());
    return scope;
}

std::vector<std::string> ItemMustKeep(const ItemOwnerInput& input)
{
    std::vector<std::string> lines = CommonMustKeep();
    lines.push_back(KIND_MUST_KEEP[static_cast<int>(PartKind(input))]);
    if (input.kind == ItemRequestKind::Set)
        lines.push_back(KIND_MUST_KEEP[static_cast<int>(ItemRequestKind::Set)]);
    return lines;
}

std::vector<std::string> ItemRenderMustKeep(const std::vector<ItemRequestTarget>& targets)
{
    std::vector<std::string> lines;
    for (const ItemRequestTarget& target : targets)
    {
        if (!target.render)
            continue;
        for (const std::string& line : target.render->mustKeep)
        {
            if (std::find(lines.begin(), lines.end(), line) == lines.end())
                lines.push_back(line);
        }
    }
    return lines;
}

std::vector<std::string> ItemRequestMustKeep(const ItemRequestDraft& draft)
{
    std::vector<std::string> lines = ItemMustKeep(draft.input);
    for (const std::string& line : ItemRenderMustKeep(draft.targets))
        lines.push_back(line);
    return lines;
}

std::string BuildItemRequestJson(const ItemRequestDraft& draft)
{
    ordered_json history = ordered_json::array();
    history.push_back({{"status", STATUS_OPEN}, {"at", draft.created}, {"by", draft.domain.filedBy}});
    ordered_json targets = ordered_json::array();
    for (const ItemRequestTarget& target : draft.targets)
        targets.push_back(TargetJson(target));

    ordered_json json;
    json["schema"] = draft.domain.schema;
    json["id"] = draft.id;
    json["created"] = draft.created;
    json["author"] = "owner (" + draft.domain.filedBy + ")";
    json["status"] = STATUS_OPEN;
    json["status_history"] = history;
    json["priority"] = PriorityName(draft.input.priority);
    json["kind"] = ItemKindName(draft.input.kind);
    json["base_commit"] = draft.baseCommit;
    json["supersedes"] = draft.supersedes ? ordered_json(*draft.supersedes) : ordered_json(nullptr);
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
