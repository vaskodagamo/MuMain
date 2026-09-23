#include "stdafx.h"

#ifdef _EDITOR

#include "ChildProcess.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>

namespace Editor
{
namespace
{
constexpr std::size_t READ_CHUNK = 4096;
constexpr char LINE_END = '\n';
constexpr char CARRIAGE_RETURN = '\r';

// Reads what a non-blocking stream has now; stops at "not ready" and at the end.
void ReadAvailable(SDL_IOStream* stream, std::string& out)
{
    if (stream == nullptr)
        return;
    char buffer[READ_CHUNK];
    for (;;)
    {
        const std::size_t count = SDL_ReadIO(stream, buffer, sizeof(buffer));
        if (count == 0)
            return;
        out.append(buffer, count);
    }
}

SDL_IOStream* StderrOf(SDL_Process* process)
{
    const SDL_PropertiesID props = SDL_GetProcessProperties(process);
    return static_cast<SDL_IOStream*>(SDL_GetPointerProperty(props, SDL_PROP_PROCESS_STDERR_POINTER, nullptr));
}
} // namespace

ChildProcess::~ChildProcess()
{
    // Closes the pipes without waiting; a tool whose stdout closes stops by itself
    // (concepts.py cancels its run), the process is not killed.
    if (m_process != nullptr)
        SDL_DestroyProcess(m_process);
}

bool ChildProcess::Start(const std::vector<std::string>& argv, std::string& error)
{
    if (m_process != nullptr || argv.empty())
    {
        error = argv.empty() ? "nothing to start" : "a process is already running";
        return false;
    }
    std::vector<const char*> args;
    for (const std::string& argument : argv)
        args.push_back(argument.c_str());
    args.push_back(nullptr);

    const SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0)
    {
        error = SDL_GetError();
        return false;
    }
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(args.data()));
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_APP);
    m_process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (m_process == nullptr)
    {
        error = SDL_GetError();
        return false;
    }
    m_partialLine.clear();
    m_exitCode.reset();
    return true;
}

void ChildProcess::Poll(std::vector<std::string>& lines, std::string& errors)
{
    if (m_process == nullptr || m_exitCode)
        return;
    ReadStdout(lines);
    ReadStderr(errors);
    int exitCode = 0;
    if (!SDL_WaitProcess(m_process, false, &exitCode))
        return;
    // Exited: what it wrote last is still in the pipes.
    ReadStdout(lines);
    ReadStderr(errors);
    if (!m_partialLine.empty())
        lines.push_back(std::move(m_partialLine));
    m_partialLine.clear();
    m_exitCode = exitCode;
}

bool ChildProcess::Terminate()
{
    if (!IsRunning())
        return false;
    return SDL_KillProcess(m_process, false);
}

void ChildProcess::ReadStdout(std::vector<std::string>& lines)
{
    ReadAvailable(SDL_GetProcessOutput(m_process), m_partialLine);
    std::size_t start = 0;
    for (std::size_t end = m_partialLine.find(LINE_END); end != std::string::npos;
         end = m_partialLine.find(LINE_END, start))
    {
        std::string line = m_partialLine.substr(start, end - start);
        if (!line.empty() && line.back() == CARRIAGE_RETURN)
            line.pop_back();
        lines.push_back(std::move(line));
        start = end + 1;
    }
    m_partialLine.erase(0, start);
}

void ChildProcess::ReadStderr(std::string& errors)
{
    ReadAvailable(StderrOf(m_process), errors);
}
} // namespace Editor

#endif // _EDITOR
