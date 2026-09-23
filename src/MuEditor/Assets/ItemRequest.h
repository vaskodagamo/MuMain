#pragma once

#ifdef _EDITOR

#include "ItemCatalog.h"
#include "RegenRequest.h" // RequestDomain, RequestPriority

#include <optional>
#include <string>
#include <utility>
#include <vector>

// An item regeneration request (schema "mu-item-regen-request/1"): the owner asks
// the art builder to rework the look of an item that is already in the game. The
// contract, every field and who may change it are in
// assets-work/Items/requests/README.md. This file builds request.json from the
// item catalog's facts and the owner's input (ItemRequestBrief.h writes brief.md);
// nothing here touches the disk or the engine.
namespace Editor::Assets
{
enum class ItemRequestKind
{
    Upscale,  // same design, better textures
    Repaint,  // new colours/materials on the same mesh
    Remodel,  // better geometry, same silhouette and function
    Redesign, // a new look for the same item
    Set,      // one of the above for the parts of one armour set
};
constexpr int ITEM_REQUEST_KIND_COUNT = 5;

// request.json's name of a kind ("upscale" ...), and back.
const char* ItemKindName(ItemRequestKind kind);
std::optional<ItemRequestKind> ParseItemKind(const std::string& name);

// Armour parts (helm, armour, pants, gloves, boots) are item groups 7..11.
bool IsArmourGroup(int group);

// What the owner typed into the Ask Codex dialog.
struct ItemOwnerInput
{
    ItemRequestKind kind = ItemRequestKind::Upscale;
    ItemRequestKind setKind = ItemRequestKind::Upscale; // kind Set only: what is done to every part
    RequestPriority priority = RequestPriority::Normal;
    std::string summary;
    std::vector<std::string> details;
    std::vector<std::string> keep;
    std::vector<std::string> avoid;
    bool pushAllowed = false; // the owner allows the worker to push its branch and open a PR
};

// The kind whose rules apply to each target: the kind itself, or setKind for a set.
ItemRequestKind PartKind(const ItemOwnerInput& input);

// One item the request changes, and the SHA-256 of each of its model files in the
// checkout at base_commit (in the catalog's model order).
struct ItemRequestTarget
{
    ItemCatalogEntry item;
    std::vector<std::string> modelSha256;
};

// One in-client capture in the request's captures/ folder.
struct ItemCaptureInfo
{
    std::string fileName;       // "01-front.jpg"
    std::string view;           // inventory, ground, equipped-front, equipped-side, turntable, glow or other
    std::optional<float> angle; // turntable: the camera's angle in degrees
    int itemLevel = 0;
    bool excellent = false;
    bool ancient = false;
    int width = 0;
    int height = 0;
    std::string clientCommit; // short sha, empty when unknown
    std::string note;
};

struct ItemRequestDraft
{
    std::string id; // <date>-<first target's key>-<slug>, also the folder name
    std::string created;
    RequestDomain domain = ItemRequestDomain();
    std::string baseCommit; // full sha
    std::optional<std::string> supersedes;
    ItemOwnerInput input;
    std::vector<ItemRequestTarget> targets; // the clicked item first
    std::vector<ItemCaptureInfo> captures;
    std::vector<std::string> referenceImages; // file names in captures/: ref-concept.jpg, ref-01.jpg, ...
};

// Every target texture used by another consumer too, with all its consumers
// (targets included; item keys by group and index, then other:<MODEL_...> by
// name); the frozen ones (a consumer outside the targets, or outside Data/Item and
// Data/Player); the files the worker may replace for `partKind`.
struct ItemRequestScope
{
    std::vector<std::pair<std::string, std::vector<std::string>>> sharedTextures; // by container
    std::vector<std::string> frozenTextures;
    std::vector<std::string> ownedFiles;
};
ItemRequestScope ComputeItemScope(ItemRequestKind partKind, const std::vector<ItemRequestTarget>& targets);

// constraints.must_keep: the README's common lines, then the kind's line (for a
// set: its part kind's line and the set line).
std::vector<std::string> ItemMustKeep(const ItemOwnerInput& input);

// request.json text, fields in the README's order, two-space indent.
std::string BuildItemRequestJson(const ItemRequestDraft& draft);
} // namespace Editor::Assets

#endif // _EDITOR
