#include "RequestFolder.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "RequestBrief.h"
#include "RequestNaming.h"

#include <json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace Editor::Assets
{
namespace
{
constexpr const char* REQUESTS_FOLDER = "requests";
constexpr const char* REQUEST_FILE = "request.json";
constexpr const char* BRIEF_FILE = "brief.md";
constexpr const char* CAPTURES_FOLDER = "captures";
constexpr const char* MODEL_EXTENSION = ".bmd";

fs::path RepoObjectDir(const fs::path& repoRoot, int world)
{
    return repoRoot / "src" / "bin" / "Data" / ("Object" + std::to_string(world));
}

std::string ReadText(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

bool WriteBytes(const fs::path& file, const void* data, std::size_t size, std::string& error)
{
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    stream.close();
    if (!stream)
        error = "cannot write " + Editor::Text::PathToUtf8(file);
    return static_cast<bool>(stream);
}

bool WriteText(const fs::path& file, const std::string& text, std::string& error)
{
    return WriteBytes(file, text.data(), text.size(), error);
}

// Removes a half-written request folder unless the write succeeded.
class FolderRollback
{
public:
    explicit FolderRollback(fs::path folder) : m_folder(std::move(folder)) {}
    ~FolderRollback()
    {
        if (m_keep)
            return;
        std::error_code ec;
        fs::remove_all(m_folder, ec);
    }
    FolderRollback(const FolderRollback&) = delete;
    FolderRollback& operator=(const FolderRollback&) = delete;
    void Keep()
    {
        m_keep = true;
    }

private:
    fs::path m_folder;
    bool m_keep = false;
};

void AddOtherNewModels(const fs::path& requestsDir, std::set<std::string>& taken)
{
    std::error_code ec;
    for (fs::directory_iterator it(requestsDir, ec), end; !ec && it != end; it.increment(ec))
    {
        const nlohmann::json request = nlohmann::json::parse(ReadText(it->path() / REQUEST_FILE), nullptr, false);
        if (!request.is_object())
            continue;
        const auto change = request.find("change");
        if (change == request.end() || !change->is_object())
            continue;
        const auto newModel = change->find("new_model");
        if (newModel != change->end() && newModel->is_string())
            taken.insert(ToLower(newModel->get<std::string>()));
    }
}
} // namespace

fs::path RequestsDir(const fs::path& repoRoot, int world)
{
    return WorldAssetsDir(repoRoot, world) / REQUESTS_FOLDER;
}

std::vector<std::string> ExistingRequestIds(const fs::path& repoRoot, int world)
{
    std::vector<std::string> ids;
    std::error_code ec;
    for (fs::directory_iterator it(RequestsDir(repoRoot, world), ec), end; !ec && it != end; it.increment(ec))
    {
        if (it->is_directory(ec))
            ids.push_back(Editor::Text::PathToUtf8(it->path().filename()));
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::set<std::string> TakenModelNames(const fs::path& repoRoot, int world, const Catalog& catalog)
{
    std::set<std::string> taken;
    for (const CatalogModel& model : catalog.models)
        taken.insert(ToLower(model.name));
    std::error_code ec;
    for (fs::directory_iterator it(RepoObjectDir(repoRoot, world), ec), end; !ec && it != end; it.increment(ec))
    {
        const fs::path& file = it->path();
        if (ToLower(Editor::Text::PathToUtf8(file.extension())) == MODEL_EXTENSION)
            taken.insert(ToLower(Editor::Text::PathToUtf8(file.stem())));
    }
    AddOtherNewModels(RequestsDir(repoRoot, world), taken);
    return taken;
}

bool WriteRequestFolder(const fs::path& repoRoot, const RequestDraft& draft, const std::vector<std::uint8_t>& jpeg,
                        fs::path& folder, std::string& error)
{
    folder = RequestsDir(repoRoot, draft.world) / draft.id;
    if (draft.captures.empty() != jpeg.empty())
    {
        error = "the capture and its image do not match";
        return false;
    }
    std::error_code ec;
    if (fs::exists(folder, ec))
    {
        error = Editor::Text::PathToUtf8(folder) + " exists already";
        return false;
    }
    const fs::path captures = folder / CAPTURES_FOLDER;
    fs::create_directories(jpeg.empty() ? folder : captures, ec);
    if (ec)
    {
        error = "cannot create " + Editor::Text::PathToUtf8(folder) + ": " + ec.message();
        return false;
    }

    FolderRollback rollback(folder);
    if (!jpeg.empty() && !WriteBytes(captures / draft.captures.front().fileName, jpeg.data(), jpeg.size(), error))
        return false;
    if (!WriteText(folder / BRIEF_FILE, BuildBrief(draft), error))
        return false;
    if (!WriteText(folder / REQUEST_FILE, BuildRequestJson(draft), error))
        return false;
    rollback.Keep();
    return true;
}
} // namespace Editor::Assets

#endif // _EDITOR
