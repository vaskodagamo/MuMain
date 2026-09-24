#include "ReferenceMesh.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "ItemRequest.h" // ITEM_MAX_TRIANGLES, ITEM_MAX_TEXTURE_SIZE

#include <system_error>

namespace fs = std::filesystem;

namespace Editor::Assets::ReferenceMesh
{
namespace
{
constexpr const char* SOURCES_FOLDER = "item-sources";
constexpr int SMALL_ITEM_TEXTURE_SIZE = 512; // enough for weapons, shields and jewels
} // namespace

fs::path Folder(const fs::path& repoRoot, const std::string& itemKey)
{
    return repoRoot.parent_path() / SOURCES_FOLDER / itemKey;
}

fs::path Store(const fs::path& source, const fs::path& repoRoot, const std::string& itemKey, std::string& error)
{
    const fs::path folder = Folder(repoRoot, itemKey);
    const fs::path stored = folder / source.filename();
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (!ec && !fs::exists(stored, ec)) // picked from there already, or stored before
        fs::copy_file(source, stored, ec);
    if (ec || !fs::is_regular_file(stored))
    {
        error = "Could not copy the reference mesh to " + Editor::Text::PathToUtf8(folder) + ": " + ec.message();
        return {};
    }
    return stored;
}

std::vector<std::string> DetailLines(const fs::path& stored)
{
    const std::string triangles = std::to_string(ITEM_MAX_TRIANGLES);
    return {
        "Start from the reference mesh " + Editor::Text::PathToUtf8(stored) +
            " (outside the repository; never copy it into the repo): it shows the wanted look",
        "Reduce it in Blender to at most " + triangles +
            " triangles per model (Decimate, then clean up), keeping the silhouette",
        "Bake its colour and detail onto one diffuse texture: " + std::to_string(SMALL_ITEM_TEXTURE_SIZE) +
            " px for small items, at most " + std::to_string(ITEM_MAX_TEXTURE_SIZE) + " px",
        "Fit it to the original model: same origin and grip, size and orientation; split a set mesh into its parts",
    };
}

std::string Summary(const std::string& itemName)
{
    return "Rebuild " + itemName + " from the reference mesh";
}
} // namespace Editor::Assets::ReferenceMesh

#endif // _EDITOR
