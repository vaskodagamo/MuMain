#pragma once

#ifdef _EDITOR

#include <filesystem>
#include <string>

// The Map Editor's "open file" dialog, shared by every import action and the
// same on every platform (SDL3).
//
// The dialog never blocks the game loop: on macOS it slides down as a sheet on
// the game window, on Windows it runs on its own thread. A button asks for a
// file with RequestOpenFile(); its tab then calls PollOpenFile() once per frame
// until the user has picked a file, cancelled, or the dialog failed.
namespace Editor::Files
{
// The editor actions that ask the user for a file (the Map Editor's imports and
// the Item Editor's reference images and A/B candidates). Each has its own result slot, so a
// pending dialog never receives another action's answer.
enum class FilePickRequest
{
    TextureImage,   // T. Browse "Upload image...":            .jpg/.jpeg/.ozj
    MinimapTga,     // Minimap "Convert a .tga to mini_map.OZT": .tga
    ServerBaseAtt,  // Attribute "Load server base .att...":   .att
    ReferenceImage, // Item Editor, Ask Codex "Add image...":  .jpg/.jpeg
    CandidateFile,  // Item Editor, A/B "Load candidate from folder...": .bmd/.ozj/.ozt
    Count
};

enum class FilePickState
{
    Idle,      // nothing requested, or the result was already polled
    Pending,   // the dialog is open
    Picked,    // the user chose a file (FilePickResult::path)
    Cancelled, // the user closed the dialog without choosing a file
    Failed     // the dialog could not be shown (FilePickResult::error)
};

struct FilePickResult
{
    FilePickState state = FilePickState::Idle;
    std::filesystem::path path;
    std::string error;
};

// Opens the dialog for `request` on top of the game window. Call it from the
// main thread (an ImGui button handler). Returns false, and opens nothing, if
// a dialog for this request is still open.
bool RequestOpenFile(FilePickRequest request);

// True while the dialog for `request` is open; use it to disable the button.
bool IsOpenFilePending(FilePickRequest request);

// Returns Picked, Cancelled or Failed exactly once per request, and Idle or
// Pending otherwise. Call it on the main thread, once per frame.
FilePickResult PollOpenFile(FilePickRequest request);
} // namespace Editor::Files

#endif // _EDITOR
