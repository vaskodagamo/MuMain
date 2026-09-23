#pragma once

#ifdef _EDITOR

#include "Core/PythonTool.h"

#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <vector>

// How the Item Editor calls tools/item_editor/concepts.py: the checkout it runs
// in, the local test API a developer may point `run` at, and one-shot `--json`
// commands on a background thread (Assets/ConceptCommands.h builds the arguments).
namespace Editor::ItemEditor::ConceptTool
{
// The checkout (empty without one: concepts need a repository).
const std::filesystem::path& Repo();

// MU_OPENAI_API_BASE when it names this machine over plain http (a mock server for
// tests); nullopt otherwise. Real runs go to the tool's own default (OpenAI).
const std::optional<std::string>& TestApiBase();

// Runs `concepts.py <arguments>` on a thread of its own; the result's `output`
// is its stdout (the JSON record), stderr goes to the editor's log stream.
std::future<Editor::PythonTool::Result> RunJson(std::vector<std::string> arguments);

// The one JSON record's text, or why there is none ("python3 not found", exit code).
struct JsonReply
{
    bool ok = false; // a record came back (it may still say "ok": false)
    std::string text;
    std::string problem;
};
JsonReply ReplyOf(const Editor::PythonTool::Result& result);

// True when `future` holds a finished result (never blocks).
bool IsReady(const std::future<Editor::PythonTool::Result>& future);
} // namespace Editor::ItemEditor::ConceptTool

#endif // _EDITOR
