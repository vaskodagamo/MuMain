#pragma once

#ifdef _EDITOR

#include "MapObjectWorld.h"
#include "MapTerrainLayers.h"

#include "Editing/CommandStack.h"

#include <memory>
#include <string>

// What the Undo/Redo buttons or keys did this frame.
enum class HistoryStep
{
    None,
    Undone,
    Redone,
};

// The Map Editor's multi-level undo/redo, shared by the Texture, Objects, Height,
// Attribute and Light tabs: one history for the loaded map, newest step last. It owns the
// targets the steps apply to (the live world objects and terrain arrays).
class CMapEditHistory
{
public:
    static CMapEditHistory& GetInstance();

    CMapObjectWorld& Objects()
    {
        return m_objects;
    }
    CMapTerrainLayers& Terrain()
    {
        return m_terrain;
    }

    // Adds a finished edit; null (an edit that changed nothing) is ignored.
    void Push(std::unique_ptr<Editor::Editing::EditCommand> command);

    // Draws "Undo: <step>" and "Redo: <step>" and takes Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z
    // and (not on a Mac) Ctrl+Y. While `editInProgress` (a stroke or drag is still
    // held) both are greyed out and the keys are ignored, not queued.
    HistoryStep Render(bool editInProgress);
    bool Undo();
    bool Redo();

    // The map was unloaded: every step belonged to it.
    void Forget();

private:
    CMapEditHistory() = default;

    bool Step(bool undo);
    void RenderButton(bool undo, bool enabled, bool& clicked);

    // Declared before the stack, so the steps that refer to them go first.
    CMapObjectWorld m_objects;
    CMapTerrainLayers m_terrain;
    Editor::Editing::CommandStack m_stack;
    std::string m_failure; // shown after a step no longer matched the map
};

#define g_MapEditHistory CMapEditHistory::GetInstance()

#endif // _EDITOR
