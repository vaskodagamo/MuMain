#include "NewMapPlan.h"

#ifdef _EDITOR

#include "EncTerrainFile.h"
#include "NamedModels.h"
#include "WorldFolders.h" // FindIgnoringCase

#include "Assets/EditorText.h"
#include "Assets/TerrainLightFile.h"
#include "MapInspect/TilePalette.h"
#include "MapScript/AttributeRules.h"

#include "World/MapInfra/CustomMapName.h"
#include "World/MapInfra/MapNumbers.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>

namespace Editor::NewMap
{
namespace
{
namespace fs = std::filesystem;
namespace Numbers = World::MapNumbers;

constexpr const char* DATA_FOLDER = "Data";
constexpr const char* WORLD_PREFIX = "World";
constexpr const char* OBJECT_PREFIX = "Object";
constexpr const char* ENC_TERRAIN_PREFIX = "EncTerrain";
constexpr const char* HEIGHT_FILE = "TerrainHeight.OZB";
constexpr const char* LIGHT_FILE = "TerrainLight.OZJ";
constexpr const char* MINIMAP_FILE = "mini_map.OZT";
constexpr const char* NAME_FILE = "MapName.txt";
// Files of a World folder that are not tile textures although they are .OZJ/.OZT: the
// light maps (a map's own, and the variants Crywolf and Battle Castle switch to) and the
// minimap, which the plan handles itself.
constexpr const char* LIGHT_PREFIX = "TerrainLight";
constexpr const char* MINIMAP_PREFIX = "mini_map";
constexpr const char* TEXTURE_EXTENSIONS[] = {".ozj", ".ozt"};
constexpr const char* MODEL_EXTENSION = ".bmd";
constexpr float MIN_LIGHT = 0.0f;
constexpr float MAX_LIGHT = 1.0f;
constexpr EncTerrainKind ENC_TERRAIN_KINDS[] = {EncTerrainKind::Mapping, EncTerrainKind::Attribute,
                                                EncTerrainKind::Objects};

std::string Utf8(const fs::path& path)
{
    return Editor::Text::PathToUtf8(path);
}

fs::path NumberedFolder(const char* prefix, int number)
{
    return fs::path(DATA_FOLDER) / (prefix + std::to_string(number));
}

std::string EncTerrainName(int folder, EncTerrainKind kind)
{
    return ENC_TERRAIN_PREFIX + std::to_string(folder) + Extension(kind);
}

bool ReadFile(const fs::path& file, Bytes& bytes)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        return false;
    bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return !stream.bad();
}

bool StartsWithIgnoringCase(const std::string& text, const std::string& prefix)
{
    return text.size() >= prefix.size() && Editor::Text::EqualIgnoringCase(text.substr(0, prefix.size()), prefix);
}

bool HasExtension(const fs::path& file, const char* extension)
{
    return Editor::Text::EqualIgnoringCase(Utf8(file.extension()), extension);
}

bool IsTexture(const fs::path& file)
{
    return std::any_of(std::begin(TEXTURE_EXTENSIONS), std::end(TEXTURE_EXTENSIONS),
                       [&file](const char* extension) { return HasExtension(file, extension); });
}

// The regular files of `dir`, sorted by name; `subfolders` gets the folders it holds.
std::vector<fs::path> FilesIn(const fs::path& dir, std::vector<std::string>* subfolders = nullptr)
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec))
    {
        if (entry.is_regular_file(ec))
            files.push_back(entry.path());
        else if (subfolders != nullptr && entry.is_directory(ec))
            subfolders->push_back(Utf8(entry.path().filename()));
    }
    std::sort(files.begin(), files.end());
    return files;
}

bool ReadNamed(const fs::path& dir, const std::string& name, Bytes& bytes, std::string& error)
{
    const std::optional<fs::path> file = FindIgnoringCase(dir, name);
    if (file && ReadFile(*file, bytes))
        return true;
    error = Utf8(dir / name) + " is missing or cannot be read";
    return false;
}

bool CheckFolderExists(const fs::path& gameRoot, const fs::path& folder, std::string& error)
{
    std::error_code ec;
    if (fs::is_directory(gameRoot / folder, ec))
        return true;
    error = Utf8(folder) + " does not exist";
    return false;
}

bool CheckName(const std::string& name, std::string& error)
{
    const std::string trimmed = Editor::Text::Trim(name);
    if (!trimmed.empty() && World::MapNames::ParseNameFile(trimmed) == trimmed)
        return true;
    error = "`name` is one line of text, at most " + std::to_string(World::MapNames::MAX_NAME_BYTES) + " bytes";
    return false;
}

void AddFile(NewMapPlan& plan, const fs::path& folder, const std::string& name, Bytes bytes)
{
    plan.files.push_back({folder / Editor::Text::Utf8Path(name), std::move(bytes)});
}

bool AddRenumberedTerrain(const fs::path& gameRoot, int sourceWorld, const MapFileCodec& codec, NewMapPlan& plan,
                          std::string& error)
{
    const fs::path source = NumberedFolder(WORLD_PREFIX, sourceWorld);
    for (const EncTerrainKind kind : ENC_TERRAIN_KINDS)
    {
        const std::string name = EncTerrainName(sourceWorld, kind);
        Bytes file;
        Bytes renumbered;
        int oldFolder = 0;
        if (!ReadNamed(gameRoot / source, name, file, error))
            return false;
        if (!Renumber(kind, file, plan.world, codec, renumbered, oldFolder, error))
        {
            error = Utf8(source / name) + ": " + error;
            return false;
        }
        if (oldFolder != sourceWorld)
            plan.warnings.push_back(name + " named folder " + std::to_string(oldFolder) + "; the copy names " +
                                    std::to_string(plan.world));
        AddFile(plan, WorldFolder(plan.world), EncTerrainName(plan.world, kind), std::move(renumbered));
    }
    return true;
}

bool AddTemplateHeightAndLight(const fs::path& gameRoot, int sourceWorld, NewMapPlan& plan, std::string& error)
{
    const fs::path source = gameRoot / NumberedFolder(WORLD_PREFIX, sourceWorld);
    Bytes height;
    Bytes light;
    if (!ReadNamed(source, HEIGHT_FILE, height, error) || !ReadNamed(source, LIGHT_FILE, light, error))
        return false;
    if (!IsEightBitHeightFile(height))
    {
        error = "World" + std::to_string(sourceWorld) +
                "'s height file stores 24 bits a corner, which only its own map number loads; pick another template";
        return false;
    }
    std::vector<float> decoded(static_cast<std::size_t>(Editor::LightMap::LIGHT_MAP_SIZE) *
                               Editor::LightMap::LIGHT_MAP_SIZE * 3);
    if (!Editor::LightMap::DecodeOzj(light, Editor::LightMap::LIGHT_MAP_SIZE, decoded.data(), error))
    {
        error = "World" + std::to_string(sourceWorld) + "/" + LIGHT_FILE + ": " + error;
        return false;
    }
    AddFile(plan, WorldFolder(plan.world), HEIGHT_FILE, std::move(height));
    AddFile(plan, WorldFolder(plan.world), LIGHT_FILE, std::move(light));
    return true;
}

// The tile textures of World{sourceWorld} (every .OZJ/.OZT but the light maps and the
// minimap), and its minimap when `withMinimap`.
bool AddTextures(const fs::path& gameRoot, int sourceWorld, bool withMinimap, NewMapPlan& plan, std::string& error)
{
    for (const fs::path& file : FilesIn(gameRoot / NumberedFolder(WORLD_PREFIX, sourceWorld)))
    {
        const std::string name = Utf8(file.filename());
        const bool minimap = Editor::Text::EqualIgnoringCase(name, MINIMAP_FILE);
        const bool texture = IsTexture(file) && !StartsWithIgnoringCase(name, LIGHT_PREFIX) &&
                             !StartsWithIgnoringCase(name, MINIMAP_PREFIX);
        if (!texture && !(minimap && withMinimap))
            continue;
        Bytes bytes;
        if (!ReadFile(file, bytes))
        {
            error = Utf8(file) + " cannot be read";
            return false;
        }
        AddFile(plan, WorldFolder(plan.world), name, std::move(bytes));
    }
    return true;
}

bool HasTextureFor(const NewMapPlan& plan, int slot)
{
    const std::string slotName = Editor::MapInspect::TileSlotName(slot);
    return std::any_of(
        plan.files.begin(), plan.files.end(), [&slotName](const PlannedFile& file)
        { return IsTexture(file.relative) && Editor::Text::EqualIgnoringCase(Utf8(file.relative.stem()), slotName); });
}

bool CheckBlankGround(const BlankGround& ground, std::string& error)
{
    std::string problem;
    if (ground.tileSlot >= Editor::MapInspect::TILE_SLOT_COUNT)
        problem = "the texture slot is 0 to " + std::to_string(Editor::MapInspect::TILE_SLOT_COUNT - 1);
    else if (!Editor::MapScript::Attributes::IsCleanValue(ground.attribute))
        problem = std::string("the walkability is one of ") + Editor::MapScript::Attributes::KnownNames();
    else if (std::any_of(ground.light.begin(), ground.light.end(),
                         [](float channel) { return !(channel >= MIN_LIGHT && channel <= MAX_LIGHT); }))
        problem = "each light channel is 0 to 1";
    if (problem.empty())
        return true;
    error = problem;
    return false;
}

bool AddBlankTerrain(const fs::path& gameRoot, const NewMapRequest& request, const MapFileCodec& codec,
                     NewMapPlan& plan, std::string& error)
{
    const BlankGround& ground = request.blank;
    const fs::path textures = gameRoot / NumberedFolder(WORLD_PREFIX, request.texturesWorld);
    Bytes templateHeight;
    Bytes height;
    if (!ReadNamed(textures, HEIGHT_FILE, templateHeight, error) ||
        !FlatHeightFile(templateHeight, ground.heightByte, height, error))
        return false;
    Bytes light = FlatLightFile(ground.light);
    if (light.empty())
    {
        error = "the light map could not be encoded";
        return false;
    }
    const fs::path folder = WorldFolder(plan.world);
    AddFile(plan, folder, EncTerrainName(plan.world, EncTerrainKind::Mapping),
            EncodeEncTerrain(EncTerrainKind::Mapping, BlankMappingPlain(plan.world, ground.tileSlot), codec));
    AddFile(plan, folder, EncTerrainName(plan.world, EncTerrainKind::Attribute),
            EncodeEncTerrain(EncTerrainKind::Attribute, BlankAttributePlain(plan.world, ground.attribute), codec));
    AddFile(plan, folder, EncTerrainName(plan.world, EncTerrainKind::Objects),
            EncodeEncTerrain(EncTerrainKind::Objects, EmptyObjectsPlain(plan.world), codec));
    AddFile(plan, folder, HEIGHT_FILE, std::move(height));
    AddFile(plan, folder, LIGHT_FILE, std::move(light));
    return true;
}

// Lorencia's named models under the generic names of their types.
void AddRenamedModels(const fs::path& source, NewMapPlan& plan)
{
    std::vector<std::string> missing;
    for (const NamedModelRun& run : LorenciaModels())
    {
        for (int index = 0; index < run.count; ++index)
        {
            const std::string name = NamedModelFile(run, index);
            const std::optional<fs::path> file = FindIgnoringCase(source, name);
            Bytes bytes;
            if (!file || !ReadFile(*file, bytes))
            {
                missing.push_back(name);
                continue;
            }
            AddFile(plan, ObjectFolder(plan.world), GenericModelFile(run.firstType + index), std::move(bytes));
        }
    }
    if (!missing.empty())
        plan.warnings.push_back("these models of Object1 are missing and were not copied: " +
                                Editor::Text::Join(missing, ", "));
}

bool AddModels(const fs::path& gameRoot, int modelsWorld, NewMapPlan& plan, std::string& error)
{
    const fs::path relative = NumberedFolder(OBJECT_PREFIX, modelsWorld);
    if (!CheckFolderExists(gameRoot, relative, error))
        return false;
    const fs::path source = gameRoot / relative;
    const bool named = modelsWorld == NAMED_MODEL_FOLDER;
    if (named)
        AddRenamedModels(source, plan);

    std::vector<std::string> subfolders;
    for (const fs::path& file : FilesIn(source, &subfolders))
    {
        if (named && HasExtension(file, MODEL_EXTENSION))
            continue; // copied under its type's name above; birds and fish are not world objects
        Bytes bytes;
        if (!ReadFile(file, bytes))
        {
            error = Utf8(file) + " cannot be read";
            return false;
        }
        AddFile(plan, ObjectFolder(plan.world), Utf8(file.filename()), std::move(bytes));
    }
    if (!subfolders.empty())
        plan.warnings.push_back("the folders inside " + Utf8(relative) +
                                " were not copied: " + Editor::Text::Join(subfolders, ", "));
    return true;
}

bool AddTemplate(const fs::path& gameRoot, const NewMapRequest& request, const MapFileCodec& codec, NewMapPlan& plan,
                 std::string& error)
{
    const int source = request.templateWorld;
    if (!CheckFolderExists(gameRoot, NumberedFolder(WORLD_PREFIX, source), error) ||
        !AddRenumberedTerrain(gameRoot, source, codec, plan, error) ||
        !AddTemplateHeightAndLight(gameRoot, source, plan, error) ||
        !AddTextures(gameRoot, source, request.copyMinimap, plan, error))
        return false;
    plan.warnings.push_back("what the game ties to World" + std::to_string(source) +
                            "'s map number does not come along: its music and sounds, fog colour, water and "
                            "object effects (Lorencia's fires and lights, for example)");
    if (request.modelsWorld != source)
        plan.warnings.push_back("the copied objects keep their model numbers, and these load from " +
                                (request.modelsWorld == 0 ? std::string("no model folder")
                                                          : "Object" + std::to_string(request.modelsWorld)) +
                                ", not from the template's Object" + std::to_string(source));
    return true;
}

bool AddBlank(const fs::path& gameRoot, const NewMapRequest& request, const MapFileCodec& codec, NewMapPlan& plan,
              std::string& error)
{
    const int textures = request.texturesWorld;
    if (!CheckBlankGround(request.blank, error) ||
        !CheckFolderExists(gameRoot, NumberedFolder(WORLD_PREFIX, textures), error) ||
        !AddBlankTerrain(gameRoot, request, codec, plan, error) || !AddTextures(gameRoot, textures, false, plan, error))
        return false;
    if (HasTextureFor(plan, request.blank.tileSlot))
        return true;
    error = "texture slot " + std::to_string(request.blank.tileSlot) + " (" +
            Editor::MapInspect::TileSlotName(request.blank.tileSlot) + ") has no file in World" +
            std::to_string(textures);
    return false;
}
} // namespace

fs::path WorldFolder(int world)
{
    return NumberedFolder(WORLD_PREFIX, world);
}

fs::path ObjectFolder(int world)
{
    return NumberedFolder(OBJECT_PREFIX, world);
}

bool CheckNewMapNumber(int map, std::string& error)
{
    if (map >= Numbers::FIRST_NEW_MAP && map <= Numbers::LAST_NEW_MAP)
        return true;
    error = "a new map takes a number from " + std::to_string(Numbers::FIRST_NEW_MAP) + " to " +
            std::to_string(Numbers::LAST_NEW_MAP) + " (folders World" + std::to_string(Numbers::FIRST_NEW_MAP + 1) +
            " to World" + std::to_string(Numbers::LAST_FOLDER) +
            "): the game's own maps use 0 to 81 (54, 55, 73, 74, 77 and 78 are the login and character "
            "scenes), the client reads a map number as one byte, and the map files repeat the folder number in "
            "one byte";
    return false;
}

bool PlanNewMap(const fs::path& gameRoot, const NewMapRequest& request, const MapFileCodec& codec, NewMapPlan& plan,
                std::string& error)
{
    if (!CheckNewMapNumber(request.map, error) || !CheckName(request.name, error))
        return false;

    plan = NewMapPlan{};
    plan.map = request.map;
    plan.world = Numbers::FolderOf(request.map);
    plan.folders = {WorldFolder(plan.world), ObjectFolder(plan.world)};
    const bool built = request.source == MapSource::Template ? AddTemplate(gameRoot, request, codec, plan, error)
                                                             : AddBlank(gameRoot, request, codec, plan, error);
    if (!built || (request.modelsWorld != 0 && !AddModels(gameRoot, request.modelsWorld, plan, error)))
        return false;
    if (request.modelsWorld == 0)
        plan.warnings.push_back("no models were copied: import them with the Map Editor's O. Browse tab (until then "
                                "each map load logs a line per missing model)");
    const std::string name = Editor::Text::Trim(request.name) + "\n";
    AddFile(plan, WorldFolder(plan.world), NAME_FILE, Bytes(name.begin(), name.end()));
    return true;
}
} // namespace Editor::NewMap

#endif // _EDITOR
