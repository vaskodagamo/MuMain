#include "ConceptCommands.h"

#ifdef _EDITOR

#include "EditorText.h"

#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace Editor::Concepts
{
namespace
{
constexpr const char* SCRIPT = "tools/item_editor/concepts.py";
constexpr std::string_view HTTP_SCHEME = "http://";
constexpr std::string_view LOOPBACK_HOSTS[] = {"127.0.0.1", "localhost"};
constexpr std::string_view LINE_JOIN = "; ";
constexpr char KEY_NOTE_SEPARATOR = '=';

void Add(std::vector<std::string>& args, std::initializer_list<std::string> values)
{
    args.insert(args.end(), values.begin(), values.end());
}

void AddRepoRoot(std::vector<std::string>& args, const fs::path& repoRoot)
{
    Add(args, {"--repo-root", Editor::Text::PathToUtf8(repoRoot)});
}

void AddApiBase(std::vector<std::string>& args, const std::optional<std::string>& apiBase)
{
    if (apiBase)
        Add(args, {"--api-base", *apiBase});
}

// The global note and the item's own, as the tool joins them; empty when neither is set.
std::string ItemNote(const GenerateSettings& settings, const std::string& key)
{
    std::vector<std::string> parts;
    if (const std::string global = OneLineNote(settings.note); !global.empty())
        parts.push_back(global);
    const auto own = settings.itemNotes.find(key);
    if (own != settings.itemNotes.end())
    {
        if (const std::string text = OneLineNote(own->second); !text.empty())
            parts.push_back(text);
    }
    return Editor::Text::Join(parts, LINE_JOIN);
}

// Always `--note KEY=text`: a note for every item is given per key, so a note that
// itself starts like "0-2=..." can never be read as another item's.
void AddSelection(std::vector<std::string>& args, const std::vector<std::string>& keys,
                  const GenerateSettings& settings)
{
    Add(args, {"--keys", Editor::Text::Join(keys, ",")});
    if (!settings.preset.empty())
        Add(args, {"--preset", settings.preset});
    if (settings.variants > 0)
        Add(args, {"--variants", std::to_string(settings.variants)});
    if (settings.sheet)
        args.emplace_back("--sheet");
    for (const std::string& key : keys)
    {
        const std::string note = ItemNote(settings, key);
        if (!note.empty())
            Add(args, {"--note", key + KEY_NOTE_SEPARATOR + note});
    }
}

void AddRefine(std::vector<std::string>& args, const VariantRef& parent, const std::string& comment)
{
    Add(args, {"--from", VariantPath(parent), "--note", parent.key + KEY_NOTE_SEPARATOR + OneLineNote(comment)});
}

bool IsDigits(std::string_view text)
{
    const auto isDigit = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };
    return !text.empty() && std::all_of(text.begin(), text.end(), isDigit);
}
} // namespace

fs::path ScriptPath(const fs::path& repoRoot)
{
    return repoRoot / Editor::Text::Utf8Path(SCRIPT);
}

std::string VariantPath(const VariantRef& variant)
{
    return variant.batch + "/" + variant.key + "/" + variant.variant;
}

std::vector<std::string> PlanArgs(const std::vector<std::string>& keys, const GenerateSettings& settings,
                                  const fs::path& repoRoot)
{
    std::vector<std::string> args = {"plan"};
    AddSelection(args, keys, settings);
    Add(args, {"--no-prompts", "--json"});
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> RunArgs(const std::vector<std::string>& keys, const GenerateSettings& settings,
                                 const fs::path& repoRoot, const std::optional<std::string>& apiBase)
{
    std::vector<std::string> args = {"run"};
    AddSelection(args, keys, settings);
    Add(args, {"--yes", "--json-progress"});
    AddApiBase(args, apiBase);
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> RefinePlanArgs(const VariantRef& parent, const std::string& comment, const fs::path& repoRoot)
{
    std::vector<std::string> args = {"plan"};
    AddRefine(args, parent, comment);
    Add(args, {"--no-prompts", "--json"});
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> RefineRunArgs(const VariantRef& parent, const std::string& comment, const fs::path& repoRoot,
                                       const std::optional<std::string>& apiBase)
{
    std::vector<std::string> args = {"run"};
    AddRefine(args, parent, comment);
    Add(args, {"--yes", "--json-progress"});
    AddApiBase(args, apiBase);
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> ResumeArgs(const std::vector<std::string>& resume, const fs::path& repoRoot,
                                    const std::optional<std::string>& apiBase)
{
    std::vector<std::string> args = resume;
    if (std::find(args.begin(), args.end(), "--json-progress") == args.end())
        args.emplace_back("--json-progress");
    AddApiBase(args, apiBase);
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> ResumeBatchArgs(const std::string& batch, const fs::path& repoRoot,
                                         const std::optional<std::string>& apiBase)
{
    return ResumeArgs({"run", "--resume", batch, "--yes"}, repoRoot, apiBase);
}

std::vector<std::string> RefsArgs(const std::vector<std::string>& keys, const fs::path& repoRoot)
{
    std::vector<std::string> args = {"refs", "--keys", Editor::Text::Join(keys, ","), "--json-progress"};
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> ListAllArgs(const fs::path& repoRoot)
{
    std::vector<std::string> args = {"list", "--json"};
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> ListKeyArgs(const std::string& key, const fs::path& repoRoot)
{
    std::vector<std::string> args = {"list", "--key", key, "--json"};
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> PickArgs(const VariantRef& variant, const fs::path& repoRoot)
{
    std::vector<std::string> args = {"pick", variant.batch, variant.key, variant.variant, "--json"};
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> UnpickArgs(const std::string& key, const fs::path& repoRoot)
{
    std::vector<std::string> args = {"unpick", key, "--json"};
    AddRepoRoot(args, repoRoot);
    return args;
}

std::vector<std::string> DiscardArgs(const VariantRef& variant, bool discard, const fs::path& repoRoot)
{
    std::vector<std::string> args = {discard ? "discard" : "undiscard", variant.batch, variant.key, variant.variant,
                                     "--json"};
    AddRepoRoot(args, repoRoot);
    return args;
}

std::optional<std::string> LoopbackApiBase(std::string_view value)
{
    if (value.substr(0, HTTP_SCHEME.size()) != HTTP_SCHEME)
        return std::nullopt;
    const std::string_view rest = value.substr(HTTP_SCHEME.size());
    const std::string_view authority = rest.substr(0, rest.find_first_of("/?#"));
    if (authority.find('@') != std::string_view::npos)
        return std::nullopt;
    const std::size_t colon = authority.find(':');
    const std::string_view host = authority.substr(0, colon);
    if (colon != std::string_view::npos && !IsDigits(authority.substr(colon + 1)))
        return std::nullopt;
    const bool loopback = std::find(std::begin(LOOPBACK_HOSTS), std::end(LOOPBACK_HOSTS), host) !=
                          std::end(LOOPBACK_HOSTS);
    if (!loopback)
        return std::nullopt;
    return std::string(value);
}

std::string OneLineNote(std::string_view text)
{
    return Editor::Text::Join(Editor::Text::NonEmptyLines(text), LINE_JOIN);
}
} // namespace Editor::Concepts

#endif // _EDITOR
