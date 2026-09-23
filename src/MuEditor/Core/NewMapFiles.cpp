#include "stdafx.h"

#ifdef _EDITOR

#include "NewMapFiles.h"

#include "NewMap/WorldFolders.h"
#include "UI/Console/MuEditorConsoleUI.h"
#include "UI/MapEditor/MapEditorFileUtil.h"

#include "Core/Globals/_crypt.h" // BuxConvert
#include "Core/Text/Utf8.h"
#include "Render/Terrain/ZzzLodTerrain.h" // MapFileEncrypt, MapFileDecrypt
#include "World/MapInfra/CustomMapName.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace Editor::NewMapFiles
{
namespace
{
namespace NewMap = Editor::NewMap;
using NewMap::Bytes;

std::filesystem::path GameRoot()
{
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::current_path(ec);
    return ec ? std::filesystem::path(".") : root;
}

void Log(const std::string& message)
{
    g_ErrorReport.Write(L"%ls\r\n", Core::Text::FromUtf8(message).c_str());
    g_MuEditorConsoleUI.LogEditor(message);
}

std::size_t FailedRepoCopies(const std::vector<NewMap::WrittenFile>& written)
{
    return static_cast<std::size_t>(
        std::count_if(written.begin(), written.end(), [](const NewMap::WrittenFile& file)
                      { return file.repo.result == Editor::Files::RepoCopyResult::Failed; }));
}

std::string Report(const CreateResult& result, bool dryRun)
{
    const NewMap::NewMapPlan& plan = result.plan;
    std::string report = (dryRun ? "Would create map " : "Created map ") + std::to_string(plan.map) + " in Data/World" +
                         std::to_string(plan.world) + " and Data/Object" + std::to_string(plan.world) + ": " +
                         std::to_string(plan.files.size()) + " files.";
    if (!dryRun)
    {
        report += "\n  game: " + Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(plan.folders.front()));
        const std::filesystem::path& repo = Editor::Files::RepoRoot().root;
        if (repo.empty())
            report += "\n  no repository copy (" + Editor::Files::RepoRoot().description + ")";
        else
            report +=
                "\n  repo: " + Editor::Files::PathToUtf8(Editor::Files::RepoDataFile(repo, plan.folders.front())) +
                " (new files: git add -f)";
        const std::size_t failed = FailedRepoCopies(result.written);
        if (failed > 0)
            report += "\n  " + std::to_string(failed) + " files could not be copied into the repository";
    }
    for (const std::string& warning : plan.warnings)
        report += "\n  note: " + warning;
    return report;
}
} // namespace

NewMap::MapFileCodec EngineCodec()
{
    NewMap::MapFileCodec codec;
    codec.encrypt = [](const Bytes& plain)
    {
        Bytes source = plain;
        Bytes file(plain.size());
        MapFileEncrypt(file.data(), source.data(), static_cast<int>(source.size()));
        return file;
    };
    codec.decrypt = [](const Bytes& file)
    {
        Bytes source = file;
        Bytes plain(file.size());
        MapFileDecrypt(plain.data(), source.data(), static_cast<int>(source.size()));
        return plain;
    };
    codec.buxConvert = [](Bytes& bytes) { BuxConvert(bytes.data(), static_cast<int>(bytes.size())); };
    return codec;
}

bool Create(const NewMap::NewMapRequest& request, bool dryRun, CreateResult& result, std::string& error)
{
    const std::filesystem::path gameRoot = GameRoot();
    const std::filesystem::path& repoRoot = Editor::Files::RepoRoot().root;
    if (!NewMap::PlanNewMap(gameRoot, request, EngineCodec(), result.plan, error) ||
        !NewMap::CheckFoldersFree(gameRoot, repoRoot, result.plan, error))
        return false;
    if (!dryRun)
    {
        if (!NewMap::WriteNewMap(gameRoot, repoRoot, Editor::Files::Timestamp(), result.plan, result.written, error))
            return false;
        World::MapNames::Forget(request.map);
        Log("[MapEditor] Created map " + std::to_string(request.map) + " (" + request.name + ") in Data/World" +
            std::to_string(result.plan.world) + ", " + std::to_string(result.written.size()) + " files");
    }
    result.report = Report(result, dryRun);
    return true;
}

std::vector<int> WorldFolders()
{
    return NewMap::ExistingWorldFolders(GameRoot());
}

int NextFreeMapNumber()
{
    return NewMap::NextFreeMapNumber(GameRoot(), Editor::Files::RepoRoot().root);
}
} // namespace Editor::NewMapFiles

#endif // _EDITOR
