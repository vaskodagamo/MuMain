#pragma once

#ifdef _EDITOR

#include <string>

// Offline world mode: "Main --editor --world N" opens the client's map folder
// Data/World{N} straight in the main scene - no server, no login, no character -
// looking through the editor's free-fly camera with the Map Editor open.
// "Main --editor --items" opens the Item Editor's studio instead (see
// ItemStudio.h): World1 is loaded but not drawn. "--items --world N" shows map N
// behind a floating Item Editor.
//
// N is the Data folder number (World1 = Lorencia, World3 = Devias). If the
// folder is missing or incomplete, the client logs why and continues with the
// normal login.
namespace Editor::OfflineWorld
{
// Remembers the map number of "--world N" (or "--world=N") and whether "--items"
// asks for the Item Editor. Only called when --editor is on the command line too.
void ReadCommandLine(const wchar_t* commandLine);

// Called once when the start-up data has loaded. Opens the requested map and
// returns true; returns false (and logs why) when no map was requested or its
// folder is incomplete, so the caller continues with the login scene.
bool TryEnter();

// Switches the offline session to Data/World{world} (control socket: map-open). The
// Map Editor drops the old map's selection and undo steps, and the camera goes to the
// new map's start view. False, with the reason in `error`, when no map is open offline
// or the folder is missing a file; the loaded map then stays as it is.
bool Open(int world, std::string& error);

// True once a map was opened offline. The client then runs without a server
// connection and without a hero, so network and game-HUD paths stay off.
bool IsActive();

// True once the offline scene was opened with "--items" and no "--world": the
// Item Editor's studio.
bool IsItemStudio();

// Switches to the free-fly camera and points it at the start view: the middle
// of the map's objects (Lorencia's town) seen from above at 45 degrees.
void ResetCamera();
} // namespace Editor::OfflineWorld

#endif // _EDITOR
