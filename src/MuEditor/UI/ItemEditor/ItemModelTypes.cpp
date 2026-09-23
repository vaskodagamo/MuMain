#include "stdafx.h"

#ifdef _EDITOR

#include "ItemModelTypes.h"

#include "Assets/ItemCandidates.h" // ParseModelConstant
#include "UI/Console/MuEditorConsoleUI.h"

#include "Core/Globals/_enum.h" // MODEL_ITEM, MODEL_*_MONK, MODEL_MASK_HELM, MAX_MODELS

#include <algorithm>
#include <string_view>

namespace Editor::ItemEditor
{
namespace
{
namespace HotReload = Assets::HotReload;

constexpr std::string_view ITEM_ROLE = "item";
constexpr std::string_view CLASS_VARIANT_ROLE = "class-variant";
// CLoadData::OpenTexture's defaults, which OpenItemTextures and OpenPlayerTextures use.
constexpr GLuint ITEM_TEXTURE_FILTER = GL_NEAREST;
constexpr GLuint ITEM_TEXTURE_WRAP = GL_REPEAT;

struct NamedSlot
{
    std::string_view name;
    int first;
};

// The armour class variants the catalog names (Rage Fighter and Dark Lord parts).
constexpr NamedSlot CLASS_VARIANT_SLOTS[] = {
    {"MODEL_HELM_MONK", MODEL_HELM_MONK},   {"MODEL_ARMOR_MONK", MODEL_ARMOR_MONK},
    {"MODEL_PANTS_MONK", MODEL_PANTS_MONK}, {"MODEL_BOOTS_MONK", MODEL_BOOTS_MONK},
    {"MODEL_MASK_HELM", MODEL_MASK_HELM},
};

HotReload::ModelRange ItemRange(int first, int end, const char* what)
{
    HotReload::ModelRange range;
    range.first = first;
    range.end = end;
    range.what = what;
    range.textureFilter = ITEM_TEXTURE_FILTER;
    range.textureWrap = ITEM_TEXTURE_WRAP;
    range.followsMap = false;
    return range;
}
} // namespace

std::optional<int> ModelTypeOf(const Assets::ItemCatalogEntry& item, const Assets::ItemModel& model)
{
    if (model.role == ITEM_ROLE)
        return MODEL_ITEM + item.Type();
    if (model.role != CLASS_VARIANT_ROLE)
        return std::nullopt;
    const auto constant = Assets::ParseModelConstant(model.modelConstant);
    if (!constant)
        return std::nullopt;
    const auto slot = std::find_if(std::begin(CLASS_VARIANT_SLOTS), std::end(CLASS_VARIANT_SLOTS),
                                   [&](const NamedSlot& named) { return named.name == constant->first; });
    if (slot == std::end(CLASS_VARIANT_SLOTS))
        return std::nullopt;
    const int type = slot->first + constant->second;
    return type < MAX_MODELS ? std::optional<int>(type) : std::nullopt;
}

const std::vector<HotReload::ModelRange>& ItemRanges()
{
    // MODEL_HELM2 .. MODEL_BOOTS_MONK follow the item block directly.
    static const std::vector<HotReload::ModelRange> ranges = {
        ItemRange(MODEL_ITEM, MODEL_BODY_HELM, "item"),
        ItemRange(MODEL_MASK_HELM, MAX_MODELS, "mask-helm"),
    };
    return ranges;
}

void AllowItemReloads()
{
    static bool allowed = false;
    if (allowed)
        return;
    allowed = true;
    for (const HotReload::ModelRange& range : ItemRanges())
    {
        if (!HotReload::AllowRange(range))
            g_MuEditorConsoleUI.LogEditor("[Items] The hot reload refused the " + range.what + " models " +
                                          std::to_string(range.first) + ".." + std::to_string(range.end - 1));
    }
}
} // namespace Editor::ItemEditor

#endif // _EDITOR
