#pragma once

#ifdef _EDITOR

#include <optional>
#include <string>
#include <vector>

struct SDL_Process;

// A child process the editor starts and keeps running while it draws frames: an
// argument list (no shell), stdin from /dev/null, stdout and stderr read without
// blocking once per frame. Terminate() asks it to stop (SIGTERM on macOS/Linux,
// so a tool can finish what it is doing; Windows has no such signal and ends it at
// once). Built on SDL's process API, which the editor already uses for its tools.
namespace Editor
{
class ChildProcess
{
public:
    ChildProcess() = default;
    ~ChildProcess();
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    // Starts `argv` (argv[0] is the program, found through PATH when not absolute).
    bool Start(const std::vector<std::string>& argv, std::string& error);

    // Everything the child wrote since the last call: complete stdout lines
    // (without the line break) are appended to `lines`, stderr text to `errors`.
    // Once the child has exited, the rest of its output is read and ExitCode() set.
    void Poll(std::vector<std::string>& lines, std::string& errors);

    bool IsStarted() const { return m_process != nullptr; }
    // Started and not yet seen to exit (Poll() notices the exit).
    bool IsRunning() const { return m_process != nullptr && !m_exitCode; }
    // The exit code once Poll() saw the child end; a negative value is the signal that ended it.
    std::optional<int> ExitCode() const { return m_exitCode; }

    // Asks the child to stop (SIGTERM). False when it is not running.
    bool Terminate();

private:
    void ReadStdout(std::vector<std::string>& lines);
    void ReadStderr(std::string& errors);

    SDL_Process* m_process = nullptr;
    std::string m_partialLine;
    std::optional<int> m_exitCode;
};
} // namespace Editor

#endif // _EDITOR
