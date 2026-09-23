// The control socket's command implementations, one file per family:
// Session (login/select-char/logout/quit), World (movement, combat, chat,
// party), Query (state/nearby/scene/screenshot/hotkey/click-ui) and, in editor
// builds, Map (map-open/map-info/map-camera, MapData: map-export/map-query,
// MapCapture: the editor's screenshots, MapEdit: map-apply/map-undo/map-redo/
// map-history/map-save/map-revert, MapNew: map-new/map-server-export, Gates:
// gate-list/gate-add/gate-remove/gate-show).
//
// Every handler answers with an encoded response line, or takes over the
// dispatcher's single act slot for a command that needs several frames.
#pragma once

#include "App/Control/ControlDispatcher.h"

#include <memory>
#include <string>
#include <string_view>

namespace App::Control::Commands
{
// Scene name as the protocol reports it: server_list, webzen, login,
// loading, character_list, world or unknown.
[[nodiscard]] std::string_view CurrentSceneName();

// Build identifier reported by `ping`, set once at start-up.
void SetBuildIdentifier(std::string identifier);
[[nodiscard]] const std::string& BuildIdentifier();

// Query family.
std::string Ping(const Request& request, std::unique_ptr<Act>& act);
std::string Scene(const Request& request, std::unique_ptr<Act>& act);
std::string State(const Request& request, std::unique_ptr<Act>& act);
std::string Nearby(const Request& request, std::unique_ptr<Act>& act);
std::string EventsSince(const Request& request, std::unique_ptr<Act>& act);
std::string WaitFor(const Request& request, std::unique_ptr<Act>& act);
std::string Screenshot(const Request& request, std::unique_ptr<Act>& act);
std::string Hotkey(const Request& request, std::unique_ptr<Act>& act);
std::string ClickUi(const Request& request, std::unique_ptr<Act>& act);

// Session family.
std::string Login(const Request& request, std::unique_ptr<Act>& act);
std::string SelectCharacter(const Request& request, std::unique_ptr<Act>& act);
std::string Logout(const Request& request, std::unique_ptr<Act>& act);
std::string Quit(const Request& request, std::unique_ptr<Act>& act);

// World family.
std::string Move(const Request& request, std::unique_ptr<Act>& act);
std::string Warp(const Request& request, std::unique_ptr<Act>& act);
std::string Teleport(const Request& request, std::unique_ptr<Act>& act);
std::string Attack(const Request& request, std::unique_ptr<Act>& act);
std::string Skill(const Request& request, std::unique_ptr<Act>& act);
std::string Pickup(const Request& request, std::unique_ptr<Act>& act);
std::string UseItem(const Request& request, std::unique_ptr<Act>& act);
std::string EquipItem(const Request& request, std::unique_ptr<Act>& act);
std::string Say(const Request& request, std::unique_ptr<Act>& act);
std::string Whisper(const Request& request, std::unique_ptr<Act>& act);
std::string Party(const Request& request, std::unique_ptr<Act>& act);
std::string Halt(const Request& request, std::unique_ptr<Act>& act);

#ifdef _EDITOR
// Map family: the Map Editor's view of the loaded map (editor builds only).
std::string MapOpen(const Request& request, std::unique_ptr<Act>& act);
std::string MapInfo(const Request& request, std::unique_ptr<Act>& act);
std::string MapCamera(const Request& request, std::unique_ptr<Act>& act);
std::string MapExport(const Request& request, std::unique_ptr<Act>& act);
std::string MapQuery(const Request& request, std::unique_ptr<Act>& act);
std::string MapTab(const Request& request, std::unique_ptr<Act>& act);

// MapEdit: edit scripts, the undo history and the map's files (editor builds only).
std::string MapApply(const Request& request, std::unique_ptr<Act>& act);
std::string MapUndo(const Request& request, std::unique_ptr<Act>& act);
std::string MapRedo(const Request& request, std::unique_ptr<Act>& act);
std::string MapHistory(const Request& request, std::unique_ptr<Act>& act);
std::string MapSave(const Request& request, std::unique_ptr<Act>& act);
std::string MapRevert(const Request& request, std::unique_ptr<Act>& act);

// MapNew and Gates: new maps, the gates between maps and the OpenMU export (editor
// builds only).
std::string MapNew(const Request& request, std::unique_ptr<Act>& act);
std::string MapServerExport(const Request& request, std::unique_ptr<Act>& act);
std::string GateList(const Request& request, std::unique_ptr<Act>& act);
std::string GateAdd(const Request& request, std::unique_ptr<Act>& act);
std::string GateRemove(const Request& request, std::unique_ptr<Act>& act);
std::string GateShow(const Request& request, std::unique_ptr<Act>& act);

// `screenshot` with `clean`, `region` or a .png `out`, which the editor's own
// capture serves.
[[nodiscard]] bool WantsEditorScreenshot(const Request& request);
std::string EditorScreenshot(const Request& request, std::unique_ptr<Act>& act);
#endif
} // namespace App::Control::Commands
