#pragma once

#ifdef _EDITOR

#include "ExportInput.h"

#include "json.hpp"

#include <string>

// The JSON files of an OpenMU export, in the shapes of OpenMU's own data model.
namespace Editor::ServerExport
{
// The gates the editor added (ExportGates), grouped by map as the Admin Panel's Map Editor
// shows them, with the property names of OpenMU's ExitGate and EnterGate. The document and
// every group carry a FormatVersion of their own, so OpenMU's Map Editor import (which
// would delete the map's monster spawns) refuses them. OpenMU's exit gates have no number,
// so each entry also names its Gate.bmd record ("ClientGate"); an enter gate's "Number" is
// its Gate.bmd number, which the client sends. "Id"s only link the entries of this file.
nlohmann::ordered_json GatesDocument(const ExportInput& input);

// The GameMapDefinition to create (or, for the game's own maps, to find): Number, Name,
// Discriminator, ExpMultiplier, the safe-zone map and where the walk map is.
nlohmann::ordered_json MapDocument(const ExportInput& input);

// The made-up id of gate `number` in these files: 6d750000-0000-4000-8000-<number>.
std::string GateId(int number);
} // namespace Editor::ServerExport

#endif // _EDITOR
