#pragma once

#ifdef _EDITOR

#include "Assets/ItemCatalog.h"
#include "Core/ModelHotReload.h"

#include <optional>
#include <vector>

// Which Models[] slots the engine draws an item's models with, and letting the
// hot reload replace them (the Item Editor's A/B compare).
namespace Editor::ItemEditor
{
// The slot of `model` of `item`: MODEL_ITEM + type for the item's own model, the
// catalog's constant for an armour class variant (MODEL_HELM_MONK + 1,
// MODEL_MASK_HELM + 5, ...). nullopt for the hand and inventory models, which the
// A/B compare does not switch.
std::optional<int> ModelTypeOf(const Assets::ItemCatalogEntry& item, const Assets::ItemModel& model);

// The blocks of Models[] the items use: every item (weapons ... armour parts,
// which load from Data/Player) with the armour class variants after them, and the
// Dark Lord's mask helms. Textures load as the game's item and player loaders
// load them; the models stay across a map change.
const std::vector<Assets::HotReload::ModelRange>& ItemRanges();

// Allows the hot reload to replace the models of ItemRanges(). Safe to call again.
void AllowItemReloads();
} // namespace Editor::ItemEditor

#endif // _EDITOR
