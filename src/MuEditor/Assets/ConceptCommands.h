#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The command lines the Item Editor runs tools/item_editor/concepts.py with
// (assets-work/Items/concepts/README.md, "Editor protocol"): argument lists for
// the interpreter, never a shell string, each ending with --repo-root. The API key
// is never part of them; concepts.py finds it itself (environment or Keychain).
namespace Editor::Concepts
{
// <repo>/tools/item_editor/concepts.py
std::filesystem::path ScriptPath(const std::filesystem::path& repoRoot);

// One "Generate concepts" round as the dialog collects it.
struct GenerateSettings
{
    std::string preset; // "explore", "final"; empty: the tool's default
    int variants = 0;   // images per item; 0: the preset's
    bool sheet = false; // front + side turnaround on one canvas
    std::string note;   // for every item
    std::map<std::string, std::string> itemNotes; // per item key, added to `note`
};

// One concept image: <batch>/<key>/<variant>, e.g. 20260923-130336-study-top-10/6-0/v2.
struct VariantRef
{
    std::string batch;
    std::string key;
    std::string variant;

    bool operator==(const VariantRef&) const = default;
};
std::string VariantPath(const VariantRef& variant);

// `plan ... --json` / `run ... --yes --json-progress` for `keys`. `apiBase` (a
// local test server, see LoopbackApiBase) is passed as --api-base when set.
std::vector<std::string> PlanArgs(const std::vector<std::string>& keys, const GenerateSettings& settings,
                                  const std::filesystem::path& repoRoot);
std::vector<std::string> RunArgs(const std::vector<std::string>& keys, const GenerateSettings& settings,
                                 const std::filesystem::path& repoRoot, const std::optional<std::string>& apiBase);

// A refine of `parent` with the owner's `comment`: its plan and its run.
std::vector<std::string> RefinePlanArgs(const VariantRef& parent, const std::string& comment,
                                        const std::filesystem::path& repoRoot);
std::vector<std::string> RefineRunArgs(const VariantRef& parent, const std::string& comment,
                                       const std::filesystem::path& repoRoot,
                                       const std::optional<std::string>& apiBase);

// Continues a cancelled or partly failed run: `resume` is the `cancelled` event's
// `resume` list (["run", "--resume", <batch>, "--yes"]).
std::vector<std::string> ResumeArgs(const std::vector<std::string>& resume, const std::filesystem::path& repoRoot,
                                    const std::optional<std::string>& apiBase);
// The same for a batch whose run finished with failures.
std::vector<std::string> ResumeBatchArgs(const std::string& batch, const std::filesystem::path& repoRoot,
                                         const std::optional<std::string>& apiBase);

// `refs --json-progress`: renders the missing reference images of `keys`.
std::vector<std::string> RefsArgs(const std::vector<std::string>& keys, const std::filesystem::path& repoRoot);

// `list --json` (every item's summary) or `list --key K --json` (one item's variants).
std::vector<std::string> ListAllArgs(const std::filesystem::path& repoRoot);
std::vector<std::string> ListKeyArgs(const std::string& key, const std::filesystem::path& repoRoot);

std::vector<std::string> PickArgs(const VariantRef& variant, const std::filesystem::path& repoRoot);
std::vector<std::string> UnpickArgs(const std::string& key, const std::filesystem::path& repoRoot);
// `discard` or `undiscard`.
std::vector<std::string> DiscardArgs(const VariantRef& variant, bool discard, const std::filesystem::path& repoRoot);

// A test API base (the environment's MU_OPENAI_API_BASE) the editor may hand to
// `run`: only plain http to this machine (127.0.0.1, localhost), e.g. a mock
// server. Anything else is refused (nullopt), so no setting can send requests or
// the key elsewhere.
std::optional<std::string> LoopbackApiBase(std::string_view value);

// A note as one argument: surrounding space trimmed, line breaks turned into "; ".
std::string OneLineNote(std::string_view text);
} // namespace Editor::Concepts

#endif // _EDITOR
