#include "RequestBrief.h"

#ifdef _EDITOR

#include <algorithm>
#include <cstdio>
#include <set>
#include <sstream>

namespace Editor::Assets
{
namespace
{
// brief.md sits in assets-work/World{N}/requests/<id>/, four folders below the repository root.
constexpr const char* REPO_ROOT_FROM_BRIEF = "../../../../";
constexpr const char* ASTRA_LINK = "ASTRA.md";
constexpr const char* README_WORKER_RULES_LINK = "../README.md#worker-rules-codex";
constexpr const char* README_LINK = "../README.md";
constexpr const char* NONE_ITEM = "- (none)\n";
constexpr std::size_t NUMBER_TEXT_CHARS = 96;

std::string RepoLink(const std::string& repoPath)
{
    return REPO_ROOT_FROM_BRIEF + repoPath;
}

std::string Code(const std::string& text)
{
    return "`" + text + "`";
}

std::string Vector3(const std::array<float, 3>& v)
{
    char text[NUMBER_TEXT_CHARS];
    std::snprintf(text, sizeof(text), "(%.1f, %.1f, %.1f)", v[0], v[1], v[2]);
    return text;
}

std::string Vector2(const std::array<double, 2>& v)
{
    char text[NUMBER_TEXT_CHARS];
    std::snprintf(text, sizeof(text), "(%.2f, %.2f)", v[0], v[1]);
    return text;
}

void BulletList(std::ostringstream& out, const std::vector<std::string>& items)
{
    if (items.empty())
        out << NONE_ITEM;
    for (const std::string& item : items)
        out << "- " << item << "\n";
}

void Header(std::ostringstream& out, const RequestDraft& draft)
{
    out << "# Regeneration request " << draft.id << "\n\n";
    out << draft.input.summary << "\n\n";
    out << "| | |\n|---|---|\n";
    out << "| Kind | " << KindName(draft.input.kind) << " |\n";
    out << "| Priority | " << PriorityName(draft.input.priority) << " |\n";
    out << "| Status | open, filed " << draft.created << " from the world editor |\n";
    out << "| Base commit | " << Code(draft.baseCommit) << " |\n";
    out << "| Deliver to | " << Code(RequestFolderPath(draft.world, draft.id) + "/delivery/") << " |\n";
    out << "| Push allowed | " << (draft.input.pushAllowed ? "yes (branch and PR on the fork, never merge)" : "no")
        << " |\n\n";
    out << "`request.json` next to this file is the contract; this brief repeats it for people. Work as "
        << "[ASTRA.md](" << RepoLink(ASTRA_LINK) << ") and the [worker rules](" << README_WORKER_RULES_LINK
        << ") describe, and check the folder with\n"
        << "`python3 assets-work/World" << draft.world << "/requests/validate_request.py "
        << RequestFolderPath(draft.world, draft.id) << "`.\n"
        << "Placement, rotation, scale and terrain are not part of this request.\n\n";
}

void OwnerNotes(std::ostringstream& out, const RequestDraft& draft)
{
    out << "## What to change\n\n" << draft.input.summary << "\n\n";
    BulletList(out, draft.input.details);
    out << "\n## Keep\n\n";
    BulletList(out, draft.input.keep);
    out << "\n## Avoid\n\n";
    BulletList(out, draft.input.avoid);
    if (draft.newModel)
    {
        out << "\n## New model\n\n"
            << "Deliver a new model " << Code(*draft.newModel) << " derived from "
            << Code(draft.targets.front().model.name) << " into `delivery/" << *draft.newModel
            << "/`. A new variant installs nothing: the owner adds the "
            << "engine table entry separately. See [New variants](" << README_LINK << "#new-variants).\n";
    }
    out << "\n";
}

void Instances(std::ostringstream& out, const RequestTarget& target)
{
    out << "- Placed " << target.model.placementCount << " times in the shipped map";
    if (target.picked.empty())
    {
        out << ".\n";
        return;
    }
    out << "; the owner picked:\n";
    for (const PickedInstance& instance : target.picked)
    {
        out << "  - at " << Vector3(instance.position) << ", tile " << Vector2(instance.tile) << ", rotation "
            << Vector3(instance.rotation) << ", scale " << instance.scale;
        if (instance.objIndex)
            out << ", record " << *instance.objIndex << " of the map's object file";
        out << "\n";
    }
}

void Textures(std::ostringstream& out, const RequestTarget& target, const RequestScope& scope)
{
    out << "- Textures:\n";
    for (const TextureLink& texture : target.model.textures)
    {
        const bool frozen = std::find(scope.frozenTextures.begin(), scope.frozenTextures.end(), texture.container) !=
                            scope.frozenTextures.end();
        const bool owned =
            std::find(scope.ownedFiles.begin(), scope.ownedFiles.end(), texture.container) != scope.ownedFiles.end();
        out << "  - " << Code(texture.name) << " in " << Code(texture.container);
        if (!texture.sharedWith.empty())
        {
            out << ", shared with";
            for (const std::string& other : texture.sharedWith)
                out << " " << other;
        }
        out << (frozen ? " (frozen)" : owned ? " (you may replace it)" : "") << "\n";
    }
}

void History(std::ostringstream& out, const CatalogModel& model)
{
    if (model.modelDir)
        out << "- Newest `source.blend`: " << Code(*model.modelDir) << " (batch " << model.lastBatch.value_or("-")
            << ")\n";
    out << "- Original: revision " << Code(model.original.revision);
    if (model.original.archive)
        out << ", archive [" << *model.original.archive << "](" << RepoLink(*model.original.archive) << ")";
    out << "\n";
    std::set<std::string> notes;
    for (auto batch = model.batches.rbegin(); batch != model.batches.rend(); ++batch)
    {
        if (!batch->notes.empty() && notes.insert(batch->notes).second)
            out << "- Notes of batch " << batch->name << ": [" << batch->notes << "](" << RepoLink(batch->notes)
                << ")\n";
    }
}

void Targets(std::ostringstream& out, const RequestDraft& draft, const RequestScope& scope)
{
    out << "## Targets\n\n";
    for (const RequestTarget& target : draft.targets)
    {
        const CatalogModel& model = target.model;
        out << "### " << model.name << " (type " << model.type << ")\n\n";
        out << model.identity << "\n\n";
        out << "- BMD: " << Code(model.bmd) << ", SHA-256 at the base commit " << Code(target.sha256) << "\n";
        Instances(out, target);
        Textures(out, target, scope);
        History(out, model);
        out << "\n";
    }
}

void Scope(std::ostringstream& out, const RequestDraft& draft, const RequestScope& scope)
{
    out << "## Files you may replace\n\n";
    BulletList(out, scope.ownedFiles);
    out << "\n## Frozen textures (keep byte-identical)\n\n";
    BulletList(out, scope.frozenTextures);
    out << "\n## Engine controls (binding)\n\n";
    bool any = false;
    for (const RequestTarget& target : draft.targets)
    {
        for (const EngineControl& control : target.model.engineControls)
        {
            any = true;
            out << "- " << target.model.name << " (" << control.sourceRow << "):";
            for (const std::string& item : control.controls)
                out << " " << Code(item);
            if (control.blendMesh)
                out << " (mesh " << *control.blendMesh << " = " << control.blendMeshTexture.value_or("?") << ")";
            out << ". " << control.requirement << "\n";
        }
    }
    if (!any)
        out << NONE_ITEM;
    out << "\n## Must keep\n\n";
    BulletList(out, MustKeepItems());
    out << "\n";
}

void Evidence(std::ostringstream& out, const RequestDraft& draft)
{
    out << "## Evidence\n\n";
    for (const CaptureInfo& capture : draft.captures)
    {
        out << "![In-client view](captures/" << capture.fileName << ")\n\n";
        out << "In-client capture, " << capture.width << "x" << capture.height << ", "
            << (capture.freeFly ? "free-fly camera" : "game camera") << " at " << Vector3(capture.cameraPosition)
            << ", angle " << Vector3(capture.cameraAngle) << ", client commit "
            << (capture.clientCommit.empty() ? std::string("unknown") : Code(capture.clientCommit)) << ".";
        if (!capture.note.empty())
            out << " " << capture.note;
        out << "\n\n";
    }
    if (draft.captures.empty())
        out << "No in-client capture was attached.\n\n";
    for (const RequestTarget& target : draft.targets)
    {
        const CatalogModel& model = target.model;
        if (!model.baselinePreview.empty())
            out << "- " << model.name << " before the rebuild (offline): [" << model.baselinePreview << "]("
                << RepoLink(model.baselinePreview) << ")\n";
        if (!model.finalPreview.empty())
            out << "- " << model.name << " now (offline): [" << model.finalPreview << "]("
                << RepoLink(model.finalPreview) << ")\n";
    }
}
} // namespace

std::string BuildBrief(const RequestDraft& draft)
{
    const RequestScope scope = ComputeScope(draft.input.kind, draft.targets);
    std::ostringstream out;
    Header(out, draft);
    OwnerNotes(out, draft);
    Targets(out, draft, scope);
    Scope(out, draft, scope);
    Evidence(out, draft);
    return out.str();
}
} // namespace Editor::Assets

#endif // _EDITOR
