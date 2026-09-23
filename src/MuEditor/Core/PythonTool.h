#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <future>
#include <string>
#include <vector>

// Runs one of the repository's Python tools (e.g. an item request's
// validate_request.py) the way the owner would in a terminal, for the editor to
// show its verdict. Uses python3 from Homebrew, /usr/local or /usr/bin, else from
// PATH (python on Windows). The editor never needs Python otherwise.
namespace Editor::PythonTool
{
struct Result
{
    bool started = false; // false: no Python interpreter could be started
    int exitCode = -1;
    std::string output; // standard output and standard error together
};

// Runs `script` with `arguments` and waits for it to finish.
Result Run(const std::filesystem::path& script, const std::vector<std::string>& arguments);

// The same on a thread of its own, so the editor keeps drawing frames; poll the
// future with wait_for(0) once per frame.
std::future<Result> RunInBackground(const std::filesystem::path& script, std::vector<std::string> arguments);
} // namespace Editor::PythonTool

#endif // _EDITOR
