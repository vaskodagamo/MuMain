#include "ItemRequestBrief.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "RequestBrief.h" // BriefText

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace Editor::Assets
{
namespace
{
using BriefText::BulletList;
using BriefText::Code;
using BriefText::RepoLink;

constexpr const char* ASTRA_LINK = "ASTRA.md";
constexpr const char* README_WORKER_RULES_LINK = "../README.md#worker-rules-codex";
constexpr const char* CATALOG_PATH = "assets-work/Items/catalog.json";
constexpr const char* RENDER_FACTS_PATH = "assets-work/Items/render-facts.json";
constexpr const char* RENDER_MODES_LINK = "assets-work/Items/README.md#how-the-game-draws-items-blending-and-effects";
constexpr const char* NOT_USED = "-";
constexpr const char* OWN_MODEL_ROLE = "item"; // the item's own model; other roles are named in the table
constexpr std::size_t NUMBER_TEXT_CHARS = 32;

bool Contains(const std::vector<std::string>& list, const std::string& value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}

std::string KindText(const ItemOwnerInput& input)
{
    std::string text = ItemKindName(input.kind);
    if (input.kind == ItemRequestKind::Set)
        text += std::string(" (") + ItemKindName(input.setKind) + " for every part)";
    return text;
}

std::string DisplayName(const ItemCatalogEntry& item)
{
    return item.name.empty() ? "(no name in the item table)" : item.name;
}

void Header(std::ostringstream& out, const ItemRequestDraft& draft)
{
    out << "# Item request " << draft.id << "\n\n";
    out << draft.input.summary << "\n\n";
    out << "| | |\n|---|---|\n";
    out << "| Kind | " << KindText(draft.input) << " |\n";
    out << "| Priority | " << PriorityName(draft.input.priority) << " |\n";
    out << "| Status | open, filed " << draft.created << " from the " << draft.domain.filedBy << " |\n";
    out << "| Base commit | " << Code(draft.baseCommit) << " |\n";
    if (draft.supersedes)
        out << "| Supersedes | " << Code(*draft.supersedes) << " |\n";
    out << "| Branch | " << Code(RequestBranch(draft.domain, draft.id)) << " |\n";
    out << "| Deliver to | " << Code(RequestDeliveryPath(draft.domain, draft.id)) << " |\n";
    out << "| Push allowed | " << (draft.input.pushAllowed ? "yes (branch and PR on the fork, never merge)" : "no")
        << " |\n\n";
    out << "`request.json` next to this file is the contract; this brief repeats it for people. Work as "
        << "[ASTRA.md](" << RepoLink(ASTRA_LINK) << ") (weapons, armour) and the [worker rules]("
        << README_WORKER_RULES_LINK << ") describe, and check the folder with\n"
        << "`python3 " << RequestsFolderPath(draft.domain) << "/validate_request.py "
        << RequestFolderPath(draft.domain, draft.id) << "`.\n"
        << "The item keeps its group, index and file names; its name, stats, size and classes are not part of "
           "this request. The +level glow, excellent shine and ancient effect are engine code.\n\n";
}

void OwnerNotes(std::ostringstream& out, const ItemRequestDraft& draft)
{
    out << "## What to change\n\n" << draft.input.summary << "\n\n";
    BulletList(out, draft.input.details);
    out << "\n## Keep\n\n";
    BulletList(out, draft.input.keep);
    out << "\n## Avoid\n\n";
    BulletList(out, draft.input.avoid);
    out << "\n";
    if (draft.referenceImages.empty())
        return;
    out << "## Reference images\n\n";
    for (const std::string& fileName : draft.referenceImages)
        out << "![" << fileName << "](captures/" << fileName << ")\n\n";
}

void Textures(std::ostringstream& out, const ItemModel& model, const ItemRequestScope& scope)
{
    for (const ItemTexture& texture : model.textures)
    {
        if (texture.container.empty())
        {
            out << "  - " << Code(texture.name) << ": no file (hidden or missing mesh texture)\n";
            continue;
        }
        const bool frozen = Contains(scope.frozenTextures, texture.container);
        const bool owned = Contains(scope.ownedFiles, texture.container);
        out << "  - " << Code(texture.name) << " in " << Code(texture.container)
            << (frozen ? " (frozen)" : owned ? " (you may replace it)" : "") << "\n";
    }
}

void Models(std::ostringstream& out, const ItemRequestTarget& target, const ItemRequestScope& scope)
{
    const ItemCatalogEntry& item = target.item;
    for (std::size_t i = 0; i < item.models.size(); ++i)
    {
        const ItemModel& model = item.models[i];
        std::string role = model.role;
        if (!model.className.empty())
            role += " (" + model.className + ")";
        const std::string sha = i < target.modelSha256.size() ? target.modelSha256[i] : "";
        out << "- " << role << ": " << Code(model.bmd) << ", " << model.triangles << " triangles, " << model.meshes
            << (model.meshes == 1 ? " mesh" : " meshes") << ", SHA-256 at the base commit " << Code(sha)
            << (Contains(scope.ownedFiles, model.bmd) ? " (you may replace it)" : "") << "\n";
        if (!model.condition.empty())
            out << "  - drawn when " << Code(model.condition) << "\n";
        Textures(out, model, scope);
    }
}

void Targets(std::ostringstream& out, const ItemRequestDraft& draft, const ItemRequestScope& scope)
{
    out << "## Targets\n\n";
    for (const ItemRequestTarget& target : draft.targets)
    {
        const ItemCatalogEntry& item = target.item;
        out << "### " << DisplayName(item) << " (" << item.key << ")\n\n";
        out << "- Family " << item.family << ", tier T" << item.tier.value << ", " << item.width << " x "
            << item.height << " inventory slots" << (item.twoHand ? ", two-handed" : "") << "\n";
        if (item.armourSet)
            out << "- Armour set " << *item.armourSet << "\n";
        Models(out, target, scope);
        out << "- Original: revision " << Code(item.originalRevision.empty() ? "-" : item.originalRevision)
            << "; every fact in [catalog.json](" << RepoLink(CATALOG_PATH) << ") under " << Code(item.key)
            << "\n\n";
    }
}

void Scope(std::ostringstream& out, const ItemRequestDraft& draft, const ItemRequestScope& scope)
{
    out << "## Files you may replace\n\n";
    BulletList(out, scope.ownedFiles);
    out << "\n## Frozen textures (keep byte-identical)\n\n";
    BulletList(out, scope.frozenTextures);
    out << "\n## Shared textures\n\n";
    std::vector<std::string> shared;
    for (const auto& [container, consumers] : scope.sharedTextures)
    {
        std::string line = Code(container) + ":";
        for (const std::string& consumer : consumers)
            line += " " + consumer;
        shared.push_back(line);
    }
    BulletList(out, shared);
    out << "\n## Must keep\n\n";
    BulletList(out, ItemRequestMustKeep(draft));
    out << "\n";
}

std::string ModeCell(const std::string& mode)
{
    return mode.empty() ? NOT_USED : mode;
}

void MeshTable(std::ostringstream& out, const ItemRenderEntry& render)
{
    out << "| Model | Mesh | Texture | Worn | Dropped | Inventory |\n|---|---|---|---|---|---|\n";
    for (const ItemModelDraw& model : render.models)
    {
        std::string file = Code(Editor::Text::PathToUtf8(Editor::Text::Utf8Path(model.bmd).filename()));
        if (model.role != OWN_MODEL_ROLE)
            file += " (" + model.role + ")";
        for (const ItemMeshDraw& mesh : model.meshes)
            out << "| " << file << " | " << mesh.mesh << " | " << Code(mesh.texture) << " | " << ModeCell(mesh.worn)
                << " | " << ModeCell(mesh.dropped) << " | " << ModeCell(mesh.inventory) << " |\n";
    }
    out << "\n";
}

void TargetRendering(std::ostringstream& out, const ItemRequestTarget& target)
{
    out << "### " << DisplayName(target.item) << " (" << target.item.key << ")\n\n";
    if (!target.render)
    {
        out << "render-facts.json has no entry for this item. Ask the owner to rebuild it "
            << "(`python3 tools/item_editor/render_facts.py`) before you paint.\n\n";
        return;
    }
    const ItemRenderEntry& render = *target.render;
    if (!render.summary.empty())
        out << render.summary << "\n\n";
    MeshTable(out, render);
    out << "What the engine adds on top (never paint it into a texture):\n\n";
    BulletList(out, render.effects);
    out << "\n";
    if (!render.mustKeep.empty())
    {
        out << "How to paint its blended and alpha meshes:\n\n";
        BulletList(out, render.mustKeep);
        out << "\n";
    }
}

void Rendering(std::ostringstream& out, const ItemRequestDraft& draft)
{
    out << "## How the game draws this item\n\n"
        << "From [render-facts.json](" << RepoLink(RENDER_FACTS_PATH) << ") (`constraints.render` in request.json); "
        << "the modes are explained in [assets-work/Items/README.md](" << RepoLink(RENDER_MODES_LINK) << "). "
        << "A blended mesh is not an opaque texture: black adds nothing (additive) or the alpha cuts it out, so "
        << "paint it for that.\n\n";
    for (const ItemRequestTarget& target : draft.targets)
        TargetRendering(out, target);
}

std::string CaptureCaption(const ItemCaptureInfo& capture)
{
    std::string caption = capture.view;
    if (capture.angle)
    {
        char angle[NUMBER_TEXT_CHARS];
        std::snprintf(angle, sizeof(angle), " at %.0f degrees", *capture.angle);
        caption += angle;
    }
    caption += ", +" + std::to_string(capture.itemLevel);
    if (capture.excellent)
        caption += " excellent";
    if (capture.ancient)
        caption += " ancient";
    caption += ", " + std::to_string(capture.width) + "x" + std::to_string(capture.height);
    if (!capture.note.empty())
        caption += ". " + capture.note;
    return caption;
}

void Evidence(std::ostringstream& out, const ItemRequestDraft& draft)
{
    out << "## Evidence\n\n";
    if (draft.captures.empty())
    {
        out << "No in-client capture was attached.\n";
        return;
    }
    const std::string commit = draft.captures.front().clientCommit;
    out << "In-client captures of the current look without the editor, client commit "
        << (commit.empty() ? std::string("unknown") : Code(commit)) << ".\n\n";
    for (const ItemCaptureInfo& capture : draft.captures)
        out << "![" << capture.fileName << "](captures/" << capture.fileName << ")\n\n" << CaptureCaption(capture)
            << "\n\n";
}
} // namespace

std::string BuildItemBrief(const ItemRequestDraft& draft)
{
    const ItemRequestScope scope = ComputeItemScope(PartKind(draft.input), draft.targets);
    std::ostringstream out;
    Header(out, draft);
    OwnerNotes(out, draft);
    Targets(out, draft, scope);
    Rendering(out, draft);
    Scope(out, draft, scope);
    Evidence(out, draft);
    return out.str();
}
} // namespace Editor::Assets

#endif // _EDITOR
