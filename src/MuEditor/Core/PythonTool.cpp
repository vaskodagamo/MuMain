#include "stdafx.h"

#ifdef _EDITOR

#include "PythonTool.h"

#include "ChildProcess.h"

#include "Assets/EditorText.h"

#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>

#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace Editor::PythonTool
{
namespace
{
#ifdef _WIN32
constexpr const char* INTERPRETERS[] = {"python", "py"};
#else
// Absolute paths first: an app started from Finder has only /usr/bin:/bin in PATH.
constexpr const char* INTERPRETERS[] = {"/opt/homebrew/bin/python3", "/usr/local/bin/python3", "/usr/bin/python3",
                                        "python3"};
#endif
// The child could not start the interpreter (posix_spawnp's "command not found").
constexpr int EXIT_NOT_FOUND = 127;

bool IsAbsent(const char* interpreter)
{
    const fs::path path(interpreter);
    std::error_code ec;
    return path.is_absolute() && !fs::exists(path, ec);
}

Result RunWith(const char* interpreter, const std::string& script, const std::vector<std::string>& arguments,
               Output outputChoice)
{
    std::vector<const char*> args = {interpreter, script.c_str()};
    for (const std::string& argument : arguments)
        args.push_back(argument.c_str());
    args.push_back(nullptr);

    Result result;
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0)
        return result;
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(args.data()));
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    if (outputChoice == Output::Merged)
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (process == nullptr)
        return result;

    std::size_t size = 0;
    int exitCode = -1;
    void* output = SDL_ReadProcess(process, &size, &exitCode);
    SDL_DestroyProcess(process);
    if (output != nullptr)
        result.output.assign(static_cast<const char*>(output), size);
    SDL_free(output);
    result.started = exitCode != EXIT_NOT_FOUND;
    result.exitCode = exitCode;
    return result;
}
} // namespace

Result Run(const fs::path& script, const std::vector<std::string>& arguments, Output output)
{
    const std::string scriptPath = Editor::Text::PathToUtf8(script);
    for (const char* interpreter : INTERPRETERS)
    {
        if (IsAbsent(interpreter))
            continue;
        Result result = RunWith(interpreter, scriptPath, arguments, output);
        if (result.started)
            return result;
    }
    return {};
}

std::future<Result> RunInBackground(const fs::path& script, std::vector<std::string> arguments, Output output)
{
    return std::async(std::launch::async, [script, arguments = std::move(arguments), output] {
        return Run(script, arguments, output);
    });
}

std::string Interpreter()
{
    for (const char* interpreter : INTERPRETERS)
    {
        if (!IsAbsent(interpreter))
            return interpreter;
    }
    return INTERPRETERS[std::size(INTERPRETERS) - 1];
}

bool StartScript(const fs::path& script, const std::vector<std::string>& arguments, ChildProcess& process,
                 std::string& error)
{
    std::vector<std::string> argv = {Interpreter(), Editor::Text::PathToUtf8(script)};
    argv.insert(argv.end(), arguments.begin(), arguments.end());
    return process.Start(argv, error);
}
} // namespace Editor::PythonTool

#endif // _EDITOR
