#pragma once

#ifdef _EDITOR

#include "ExportInput.h"

#include <string>

// HOWTO.md of an OpenMU export: the Admin Panel steps that set the exported map up on
// the server, with this map's numbers, names, gates and files filled in.
namespace Editor::ServerExport
{
std::string HowToText(const ExportInput& input);
} // namespace Editor::ServerExport

#endif // _EDITOR
