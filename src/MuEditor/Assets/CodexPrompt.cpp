#include "CodexPrompt.h"

#ifdef _EDITOR

namespace Editor::Assets::CodexPrompt
{
namespace
{
constexpr const char* TEMPLATE_FILE_NAME = "codex-prompt.md";
constexpr const char* PROMPT_HEADING = "## prompt\n";
constexpr const char* BRANCH_PREFIX = "codex/";

void ReplaceAll(std::string& text, const std::string& placeholder, const std::string& value)
{
    for (std::size_t at = text.find(placeholder); at != std::string::npos; at = text.find(placeholder, at + value.size()))
        text.replace(at, placeholder.size(), value);
}

std::string Trimmed(const std::string& text)
{
    const std::size_t first = text.find_first_not_of(" \n\r\t");
    const std::size_t last = text.find_last_not_of(" \n\r\t");
    return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}
} // namespace

std::filesystem::path TemplateFile(const std::filesystem::path& requestsDir)
{
    return requestsDir / TEMPLATE_FILE_NAME;
}

std::string WorkerLabel(const std::string& branch)
{
    const std::string prefix = BRANCH_PREFIX;
    return branch.rfind(prefix, 0) == 0 ? branch.substr(prefix.size()) : branch;
}

std::string Fill(const std::string& templateText, const std::string& id, const std::string& branch)
{
    const std::size_t heading = templateText.find(PROMPT_HEADING);
    if (heading == std::string::npos)
        return {};
    std::string prompt = Trimmed(templateText.substr(heading + std::string(PROMPT_HEADING).size()));
    ReplaceAll(prompt, "$id", id);
    ReplaceAll(prompt, "$label", WorkerLabel(branch));
    ReplaceAll(prompt, "$branch", branch);
    return prompt;
}
} // namespace Editor::Assets::CodexPrompt

#endif // _EDITOR
