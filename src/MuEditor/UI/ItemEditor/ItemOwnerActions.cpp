#include "stdafx.h"

#ifdef _EDITOR

#include "ItemOwnerActions.h"

#include "ItemRequestDialog.h"
#include "ItemRequestWatch.h"

#include "Assets/ClientReview.h"
#include "Assets/EditorText.h"
#include "Assets/RequestNaming.h"
#include "Core/EditorFiles.h"
#include "UI/Console/MuEditorConsoleUI.h"

#include "imgui.h"

namespace Editor::ItemEditor
{
namespace
{
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};
constexpr ImVec4 ERROR_COLOR{1.0f, 0.45f, 0.4f, 1.0f};
constexpr std::size_t NOTE_CHARS = 256;

// client-review.json as last read or written; read again after every verdict.
struct VerdictFile
{
    bool loaded = false;
    Assets::ClientReviews reviews;
    std::string error;
    std::string noteKey; // the item the note field belongs to
    char note[NOTE_CHARS] = {};
};

VerdictFile& Verdicts()
{
    static VerdictFile file;
    const std::filesystem::path& repo = Editor::Files::RepoRoot().root;
    if (!file.loaded && !repo.empty())
    {
        file.loaded = true;
        file.reviews = Assets::ReadClientReviews(Assets::ItemClientReviewFile(repo), file.error);
    }
    return file;
}

void RecordVerdict(VerdictFile& file, const std::string& key, const char* verdict)
{
    const std::filesystem::path path = Assets::ItemClientReviewFile(Editor::Files::RepoRoot().root);
    const Assets::ClientReview review{verdict, Editor::Text::Trim(file.note), Assets::CurrentTimestamp().date};
    file.error.clear();
    if (!Assets::RecordClientReview(path, key, review, file.error))
        return;
    file.reviews[key] = review;
    g_MuEditorConsoleUI.LogEditor("[Items] " + key + ": " + verdict + " in " + Editor::Text::PathToUtf8(path));
}

void RenderVerdict(const std::string& key)
{
    VerdictFile& file = Verdicts();
    if (file.noteKey != key)
    {
        file.noteKey = key;
        file.note[0] = '\0';
    }
    const auto current = file.reviews.find(key);
    if (current != file.reviews.end())
        ImGui::TextColored(NOTE_COLOR, "Your verdict: %s (%s)%s%s", current->second.verdict.c_str(),
                           current->second.date.c_str(), current->second.note.empty() ? "" : " - ",
                           current->second.note.c_str());
    if (ImGui::Button("Looks good"))
        RecordVerdict(file, key, Assets::VERDICT_LOOKS_GOOD);
    ImGui::SameLine();
    if (ImGui::Button("Needs work"))
        RecordVerdict(file, key, Assets::VERDICT_NEEDS_WORK);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##VerdictNote", "note (optional)", file.note, sizeof(file.note));
    if (!file.error.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", file.error.c_str());
}

void RenderLiveRequests(const std::string& key)
{
    for (const Assets::ItemRequestSummary* request : g_ItemRequestWatch.LiveRequestsFor(key))
    {
        const char* delivered = request->deliveryPresent ? ", delivery present" : "";
        ImGui::BulletText("%s: %s %s%s", request->id.c_str(), request->kind.c_str(), request->status.c_str(),
                          delivered);
    }
}
} // namespace

void RenderOwnerActions(const Assets::ItemCatalogEntry& item, const Assets::ItemCatalog& catalog)
{
    ImGui::SeparatorText("Your verdict and requests");
    if (Editor::Files::RepoRoot().root.empty())
    {
        ImGui::TextColored(NOTE_COLOR, "No repository checkout: verdicts and requests need one.");
        return;
    }
    RenderVerdict(item.key);
    RenderLiveRequests(item.key);
    if (ImGui::Button("Ask Codex..."))
        g_ItemRequestDialog.Open(item, catalog);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ask the art builder to upscale, repaint, remodel or redesign this item.");
}
} // namespace Editor::ItemEditor

#endif // _EDITOR
