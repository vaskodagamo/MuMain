#include "stdafx.h"

#ifdef _EDITOR

#include "MapEditorFilePicker.h"

#include "Core/MuEditorCore.h" // the game window the dialog attaches to

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>

#include <array>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <utility>

namespace Editor::Files
{
namespace
{
constexpr int REQUEST_COUNT = static_cast<int>(FilePickRequest::Count);

// SDL reads the filters after RequestOpenFile() has returned (until the dialog
// closes), so they live in static storage. "All files" matches the old Win32
// dialogs; on macOS any "*" entry lifts the filter, so every file is choosable
// there and the loaders report a wrong format instead.
constexpr SDL_DialogFileFilter TEXTURE_FILTERS[] = {
    {"Textures (JPEG, OZJ)", "jpg;jpeg;ozj"},
    {"All files", "*"},
};
constexpr SDL_DialogFileFilter MINIMAP_TGA_FILTERS[] = {
    {"TGA image", "tga"},
    {"All files", "*"},
};
constexpr SDL_DialogFileFilter SERVER_ATT_FILTERS[] = {
    {"Server TerrainData", "att"},
    {"All files", "*"},
};
constexpr SDL_DialogFileFilter REFERENCE_IMAGE_FILTERS[] = {
    {"JPEG images", "jpg;jpeg"},
    {"All files", "*"},
};

struct DialogSpec
{
    const char* title;
    const SDL_DialogFileFilter* filters;
    int filterCount;
};

// Indexed by FilePickRequest.
constexpr std::array<DialogSpec, REQUEST_COUNT> DIALOG_SPECS = {{
    {"Select a texture to import (JPEG or OZJ)", TEXTURE_FILTERS, static_cast<int>(std::size(TEXTURE_FILTERS))},
    {"Select your edited minimap .tga", MINIMAP_TGA_FILTERS, static_cast<int>(std::size(MINIMAP_TGA_FILTERS))},
    {"Select the server's current TerrainData (downloaded from the Admin Panel)", SERVER_ATT_FILTERS,
     static_cast<int>(std::size(SERVER_ATT_FILTERS))},
    {"Select a reference image for the request (JPEG)", REFERENCE_IMAGE_FILTERS,
     static_cast<int>(std::size(REFERENCE_IMAGE_FILTERS))},
}};

// One result slot per request. SDL may run the dialog callback on another
// thread (Windows shows the dialog on its own thread), so every access to the
// slots holds s_slotsMutex.
struct PickSlot
{
    FilePickState state = FilePickState::Idle;
    std::string utf8Path;
    std::string error;
};

std::mutex s_slotsMutex;
std::array<PickSlot, REQUEST_COUNT> s_slots;

bool IsValidRequest(FilePickRequest request)
{
    const int index = static_cast<int>(request);
    return index >= 0 && index < REQUEST_COUNT;
}

// SDL hands over UTF-8; std::filesystem reads UTF-8 through char8_t on every
// platform (a plain char string would use the Windows ANSI code page).
std::filesystem::path PathFromUtf8(const std::string& utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

void FailSlot(int index, const char* error)
{
    std::lock_guard<std::mutex> lock(s_slotsMutex);
    s_slots[index].state = FilePickState::Failed;
    s_slots[index].error = (error != nullptr) ? error : "";
}

// SDL_DialogFileCallback: a null `filelist` means an error, an empty list
// (first entry null) means the user cancelled. `userdata` is the slot index.
void SDLCALL OnDialogClosed(void* userdata, const char* const* filelist, int /*filter*/)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(userdata));
    if (index < 0 || index >= REQUEST_COUNT)
        return;

    if (filelist == nullptr)
    {
        FailSlot(index, SDL_GetError());
        return;
    }

    std::lock_guard<std::mutex> lock(s_slotsMutex);
    PickSlot& slot = s_slots[index];
    if (filelist[0] == nullptr)
    {
        slot.state = FilePickState::Cancelled;
        return;
    }
    slot.state = FilePickState::Picked;
    slot.utf8Path = filelist[0];
}

void ShowDialog(int index)
{
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0)
    {
        FailSlot(index, SDL_GetError());
        return;
    }

    // The filters are only read by SDL; the property API just takes void*.
    const DialogSpec& spec = DIALOG_SPECS[index];
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER,
                           const_cast<SDL_DialogFileFilter*>(spec.filters));
    SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, spec.filterCount);
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, spec.title);
    // With a parent window macOS shows a sheet; without one it would run a
    // modal loop that freezes the frame in the middle of the ImGui pass.
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, g_MuEditorCore.GetWindow());

    void* userdata = reinterpret_cast<void*>(static_cast<std::intptr_t>(index));
    SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFILE, OnDialogClosed, userdata, props);
    SDL_DestroyProperties(props);
}
} // namespace

bool RequestOpenFile(FilePickRequest request)
{
    if (!IsValidRequest(request))
        return false;

    const int index = static_cast<int>(request);
    {
        std::lock_guard<std::mutex> lock(s_slotsMutex);
        if (s_slots[index].state == FilePickState::Pending)
            return false;
        s_slots[index] = PickSlot{FilePickState::Pending};
    }

    // Not under the lock: SDL may call OnDialogClosed before returning (errors).
    ShowDialog(index);
    return true;
}

bool IsOpenFilePending(FilePickRequest request)
{
    if (!IsValidRequest(request))
        return false;

    std::lock_guard<std::mutex> lock(s_slotsMutex);
    return s_slots[static_cast<int>(request)].state == FilePickState::Pending;
}

FilePickResult PollOpenFile(FilePickRequest request)
{
    FilePickResult result;
    if (!IsValidRequest(request))
        return result;

    std::lock_guard<std::mutex> lock(s_slotsMutex);
    PickSlot& slot = s_slots[static_cast<int>(request)];
    result.state = slot.state;
    if (slot.state == FilePickState::Idle || slot.state == FilePickState::Pending)
        return result;

    result.path = PathFromUtf8(slot.utf8Path);
    result.error = std::move(slot.error);
    slot = PickSlot{};
    return result;
}

} // namespace Editor::Files

#endif // _EDITOR
