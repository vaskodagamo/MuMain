#include "stdafx.h"

#ifdef _EDITOR

#include "ConceptTool.h"

#include "Assets/ConceptCommands.h"
#include "Core/EditorFiles.h"

#include <chrono>
#include <cstdlib>

namespace Editor::ItemEditor::ConceptTool
{
namespace
{
constexpr const char* TEST_API_BASE_VARIABLE = "MU_OPENAI_API_BASE";
constexpr int EXIT_NOT_FOUND = 127;
} // namespace

const std::filesystem::path& Repo()
{
    return Editor::Files::RepoRoot().root;
}

const std::optional<std::string>& TestApiBase()
{
    static const std::optional<std::string> base = [] {
        const char* value = std::getenv(TEST_API_BASE_VARIABLE);
        return value != nullptr ? Editor::Concepts::LoopbackApiBase(value) : std::nullopt;
    }();
    return base;
}

std::future<Editor::PythonTool::Result> RunJson(std::vector<std::string> arguments)
{
    return Editor::PythonTool::RunInBackground(Editor::Concepts::ScriptPath(Repo()), std::move(arguments),
                                               Editor::PythonTool::Output::StdoutOnly);
}

JsonReply ReplyOf(const Editor::PythonTool::Result& result)
{
    JsonReply reply;
    if (!result.started || result.exitCode == EXIT_NOT_FOUND)
    {
        reply.problem = "python3 could not be started (install the Command Line Tools or Homebrew python3).";
        return reply;
    }
    if (result.output.empty())
    {
        reply.problem = "concepts.py wrote no answer (exit " + std::to_string(result.exitCode) + "); see the log.";
        return reply;
    }
    reply.ok = true;
    reply.text = result.output;
    return reply;
}

bool IsReady(const std::future<Editor::PythonTool::Result>& future)
{
    using namespace std::chrono_literals;
    return future.valid() && future.wait_for(0s) == std::future_status::ready;
}
} // namespace Editor::ItemEditor::ConceptTool

#endif // _EDITOR
