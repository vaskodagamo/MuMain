#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <future>
#include <string>
#include <vector>

namespace Editor
{
class ChildProcess;
}

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
    std::string output; // standard output (and standard error, with Output::Merged)
};

enum class Output
{
    Merged,     // standard error mixed into `output` (a report for the owner)
    StdoutOnly, // standard error goes to the editor's own (a tool that answers in JSON on stdout)
};

// Runs `script` with `arguments` and waits for it to finish.
Result Run(const std::filesystem::path& script, const std::vector<std::string>& arguments,
           Output output = Output::Merged);

// The same on a thread of its own, so the editor keeps drawing frames; poll the
// future with wait_for(0) once per frame.
std::future<Result> RunInBackground(const std::filesystem::path& script, std::vector<std::string> arguments,
                                    Output output = Output::Merged);

// The interpreter Run() tries first: the first of the absolute paths that exists,
// else the bare name (found through PATH).
std::string Interpreter();

// Starts `script` with `arguments` as a long-running child the caller polls
// (ChildProcess.h), with Interpreter().
bool StartScript(const std::filesystem::path& script, const std::vector<std::string>& arguments,
                 ChildProcess& process, std::string& error);
} // namespace Editor::PythonTool

#endif // _EDITOR
