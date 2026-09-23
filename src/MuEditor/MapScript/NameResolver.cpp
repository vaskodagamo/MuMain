#include "NameResolver.h"

#ifdef _EDITOR

#include "MapInspect/TilePalette.h" // TileSlotName

#include <algorithm>
#include <cctype>
#include <variant>

namespace Editor::MapScript
{
namespace
{
// An error lists at most this many of the names the map has.
constexpr std::size_t LISTED_NAMES = 24;

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// "TileGrass01" from "World1\TileGrass01.jpg".
std::string FileStem(const std::string& file)
{
    const std::size_t slash = file.find_last_of("\\/");
    std::string name = slash == std::string::npos ? file : file.substr(slash + 1);
    const std::size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

const ModelInfo* FindModelType(const MapContext& context, int type)
{
    const auto found = std::find_if(context.models.begin(), context.models.end(),
                                    [type](const ModelInfo& model) { return model.type == type; });
    return found == context.models.end() ? nullptr : &*found;
}

std::string KnownModels(const MapContext& context)
{
    std::string list;
    for (std::size_t i = 0; i < context.models.size() && i < LISTED_NAMES; ++i)
    {
        const int type = context.models[i].type;
        list += (i == 0 ? "" : ", ") + ModelDisplayName(context, type) + " (type " + std::to_string(type) + ")";
    }
    if (context.models.size() > LISTED_NAMES)
        list += " and " + std::to_string(context.models.size() - LISTED_NAMES) +
                " more (map-info with \"models\": true, mapctl info --models, lists every one with its type)";
    return list.empty() ? "none" : list;
}

bool ResolveModel(ModelChoice& model, const MapContext& context, const std::string& where, std::string& error)
{
    if (model.name.empty())
    {
        if (FindModelType(context, model.type) != nullptr)
            return true;
        error = where + ": model type " + std::to_string(model.type) +
                " is not loaded on this map; loaded: " + KnownModels(context);
        return false;
    }
    const std::string wanted = Lower(model.name);
    std::vector<int> types;
    for (const ModelInfo& info : context.models)
    {
        const bool named = Lower(info.catalogName) == wanted || Lower(info.modelName) == wanted;
        if (named && std::find(types.begin(), types.end(), info.type) == types.end())
            types.push_back(info.type);
    }
    if (types.size() == 1)
    {
        model.type = types.front();
        return true;
    }
    if (types.empty())
    {
        error =
            where + ": no model called \"" + model.name + "\" is loaded on this map; loaded: " + KnownModels(context);
        return false;
    }
    error = where + ": \"" + model.name + "\" names several models (types";
    for (int type : types)
        error += " " + std::to_string(type);
    error += "); give the type number instead";
    return false;
}

bool ResolveModels(std::vector<ModelChoice>& models, const MapContext& context, const std::string& where,
                   std::string& error)
{
    for (std::size_t i = 0; i < models.size(); ++i)
    {
        const std::string field = models.size() == 1 ? where + ".model" : where + ".models[" + std::to_string(i) + "]";
        if (!ResolveModel(models[i], context, field, error))
            return false;
    }
    return true;
}

bool SlotLoaded(const MapContext& context, int slot)
{
    return std::any_of(context.tileSlots.begin(), context.tileSlots.end(),
                       [slot](const Editor::MapInspect::TileSlotInfo& info)
                       { return info.slot == slot && !info.file.empty(); });
}

std::string KnownTiles(const MapContext& context)
{
    std::string list;
    for (const Editor::MapInspect::TileSlotInfo& info : context.tileSlots)
    {
        if (!info.file.empty())
            list += (list.empty() ? "" : ", ") + std::to_string(info.slot) + " " +
                    Editor::MapInspect::TileSlotName(info.slot);
    }
    return list.empty() ? "none" : list;
}

bool ResolveTile(TileChoice& tile, const MapContext& context, const std::string& where, std::string& error)
{
    if (tile.name.empty())
    {
        if (SlotLoaded(context, tile.slot))
            return true;
        error = where + ": texture slot " + std::to_string(tile.slot) +
                " holds no texture on this map; it has: " + KnownTiles(context);
        return false;
    }
    const std::string wanted = Lower(tile.name);
    for (const Editor::MapInspect::TileSlotInfo& info : context.tileSlots)
    {
        const bool named = Lower(Editor::MapInspect::TileSlotName(info.slot)) == wanted ||
                           (!info.file.empty() && Lower(FileStem(info.file)) == wanted);
        if (named && !info.file.empty())
        {
            tile.slot = info.slot;
            return true;
        }
    }
    error = where + ": no texture \"" + tile.name + "\" is loaded on this map; it has: " + KnownTiles(context);
    return false;
}

std::string OpPath(const Op& op)
{
    return "ops[" + std::to_string(op.index) + "]";
}

bool ResolveScatter(ScatterEdit& edit, const MapContext& context, const std::string& path, std::string& error)
{
    if (!ResolveModels(edit.models, context, path, error))
        return false;
    for (std::size_t i = 0; i < edit.avoid.textures.size(); ++i)
    {
        if (!ResolveTile(edit.avoid.textures[i], context, path + ".avoid.textures[" + std::to_string(i) + "]", error))
            return false;
    }
    return true;
}

bool ResolveOp(Op& op, const MapContext& context, std::string& error)
{
    const std::string path = OpPath(op);
    if (auto* texture = std::get_if<TextureEdit>(&op.data))
        return op.kind == OpKind::TextureErase || ResolveTile(texture->tile, context, path + ".tile", error);
    if (auto* place = std::get_if<PlaceEdit>(&op.data))
        return ResolveModel(place->model, context, path + ".model", error);
    if (auto* scatter = std::get_if<ScatterEdit>(&op.data))
        return ResolveScatter(*scatter, context, path, error);
    if (auto* edit = std::get_if<ObjectEdit>(&op.data))
        return ResolveModels(edit->select.models, context, path + ".select", error);
    if (auto* attribute = std::get_if<AttributeEdit>(&op.data); attribute != nullptr && attribute->under)
        return ResolveModels(attribute->under->models, context, path + ".under", error);
    return true;
}
} // namespace

bool ResolveNames(EditScript& script, const MapContext& context, std::string& error)
{
    for (Op& op : script.ops)
    {
        if (!ResolveOp(op, context, error))
            return false;
    }
    return true;
}

std::string ModelDisplayName(const MapContext& context, int type)
{
    const ModelInfo* model = FindModelType(context, type);
    if (model != nullptr && !model->catalogName.empty())
        return model->catalogName;
    if (model != nullptr && !model->modelName.empty())
        return model->modelName;
    return "type " + std::to_string(type);
}
} // namespace Editor::MapScript

#endif // _EDITOR
