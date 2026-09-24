#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <string>

// The prompt the owner pastes into a Codex session to start the art builder on
// one item request. Its text is the owner's file
// assets-work/Items/requests/codex-prompt.md: everything below the "## prompt"
// heading, with $id, $label and $branch filled in from the request.
namespace Editor::Assets::CodexPrompt
{
std::filesystem::path TemplateFile(const std::filesystem::path& requestsDir);

// The worker label of a request's branch: "codex/item-req-0-1-x" -> "item-req-0-1-x".
std::string WorkerLabel(const std::string& branch);

// The prompt from the template text; empty when it has no "## prompt" heading.
std::string Fill(const std::string& templateText, const std::string& id, const std::string& branch);
} // namespace Editor::Assets::CodexPrompt

#endif // _EDITOR
