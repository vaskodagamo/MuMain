#pragma once

#ifdef _EDITOR

#include "ExportInput.h"

#include <string>

// openmu.sql of an OpenMU export: the Admin Panel steps of HOWTO.md as one PostgreSQL
// transaction against OpenMU's schema (config."GameMapDefinition", "ExitGate",
// "EnterGate", "GameServerConfigurationGameMapDefinition"). The editor never runs it;
// its first lines say so, and psql stops at its first error. A new map is created only
// when its number is not in the database yet; the gates are the ones the editor added
// (ExportGates): their exit gates are found by map and area and created when missing, a
// game's own arrival they land on is only looked up, and enter gates are created when
// their map has none of that number. Free text (the map name) is written as hex bytes, so
// no name can end the script's quoting.
namespace Editor::ServerExport
{
std::string SqlScript(const ExportInput& input);
} // namespace Editor::ServerExport

#endif // _EDITOR
