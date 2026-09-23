#pragma once

#ifdef _EDITOR

#include "TransformGizmo.h"

#include "Editing/ObjectSelection.h"
#include "Editing/ObjectTransform.h"
#include "Editing/ObjectWorld.h"

#include "Core/Globals/_types.h" // vec3_t

#include <functional>
#include <string>
#include <utility>
#include <vector>

class OBJECT;

// The Objects tab's Select & edit mode: the selected objects (kept across tabs until
// the map changes), picking and dragging them in the world, the transform gizmo,
// their fields, and Drop to ground, Duplicate and Delete. Every edit goes into the
// Map Editor's undo history as one step.
class CMapObjectEditor
{
public:
    static CMapObjectEditor& GetInstance();

    // This frame's input for the world: the ground point under the cursor, the
    // captured left button and whether the cursor is over the world (not a panel).
    struct WorldInput
    {
        bool groundValid = false;
        vec3_t ground = {};
        bool leftDown = false;
        bool overWorld = false;
    };

    Editor::Editing::ObjectSelection& Selection()
    {
        return m_selection;
    }
    OBJECT* Primary() const
    {
        return m_selection.Primary();
    }

    // The gizmo's keys (W, E, R) and the gizmo, then picks, multi-selects and drags
    // objects under the cursor. Call once per frame while the Objects tab's Select &
    // edit mode is on.
    void HandleWorldInput(const WorldInput& input);
    // Before the game reads the keyboard (CMuEditorCore::Update): a gizmo key or Esc
    // meant for the editor must not also reach the game, whose Q/W/E/R are potion
    // hotkeys and whose Esc opens the menu.
    void WithholdKeysFromGame();

    void SetGizmoMode(GizmoMode mode)
    {
        m_gizmo.SetMode(mode);
    }
    GizmoMode GetGizmoMode() const
    {
        return m_gizmo.Mode();
    }
    bool IsSnapping() const
    {
        return m_snap;
    }
    void SetSnapping(bool snap)
    {
        m_snap = snap;
    }
    // Moves, turns or scales the selection around its centre as one undo step, as a
    // gizmo drag of `delta` does (with snapping when it is on).
    void ApplyTransform(const Editor::Editing::Transform::Delta& delta);
    // The selected objects' fields and actions (the primary's values; an edit applies
    // as a change to every selected object).
    void RenderSelectionPanel(int world, bool& showInAssetsTab);
    // Selects `objects` from outside the tab (the Outliner, the Assets tab).
    void Select(const std::vector<OBJECT*>& objects);
    // Adds `object` to the selection, or takes it out (a Shift/Cmd click in the Outliner).
    void Toggle(OBJECT* object);

    // A drag or a field edit has not finished yet: undo and redo wait for it.
    bool IsEditInProgress() const;
    // Once at the end of every Map Editor frame: a drag whose world input stopped
    // (another tab or mode, the panel closed) ends, and so does a field edit whose
    // field was not drawn.
    void EndFrame();
    // After an undo or redo: selects the objects it changed or created, in the step's
    // own order (an undo applies it backwards), so the primary stays the primary.
    void OnHistoryStep(bool undone);
    // The map's objects were freed.
    void Forget();
    // Hands the selection to the world renderer's outline, or clears it.
    void PublishOutline(bool show);
    // A status line for the Objects tab (last action).
    const std::string& Status() const
    {
        return m_status;
    }
    void SetStatus(std::string status)
    {
        m_status = std::move(status);
    }

    void DeleteSelection();
    void DuplicateSelection();
    void DropSelectionToGround();

private:
    CMapObjectEditor() = default;

    enum class Field
    {
        None,
        Position,
        Angle,
        Scale,
    };

    void HandleGizmoKeys(const WorldInput& input);
    // Runs the gizmo; true while it has the mouse (a handle under the cursor or a drag).
    bool UpdateGizmo(const WorldInput& input);
    void BeginGizmoDrag();
    void ApplyGizmoDelta(const Editor::Editing::Transform::Delta& delta);
    void EndGizmoDrag(const Editor::Editing::Transform::Delta& delta);
    void CancelGizmoDrag();
    void RenderGizmoControls();
    void PressOnObject(OBJECT* hit, const WorldInput& input);
    void BeginGroundDrag(const WorldInput& input);
    void UpdateGroundDrag(const WorldInput& input);
    void EndGroundDrag();
    void CancelGroundDrag();
    void RenderTransformFields();
    void RenderActions();
    // Applies `states` to the selected objects (same order), keeping the selection
    // valid when an object is re-created in another block.
    void ApplyToSelection(const std::vector<Editor::Editing::ObjectState>& states);
    std::vector<Editor::Editing::ObjectState> SelectionStates() const;
    // After a field widget: applies its change to every selected object with
    // `change`, and makes the whole time the field is active one undo step.
    void HandleFieldWidget(Field field, const char* label, bool changed,
                           const std::function<void(Editor::Editing::ObjectState&)>& change);
    void BeginFieldEdit(Field field, const char* label);
    void EndFieldEdit(Field field);
    void CommitFieldEdit();
    // Pushes the move of the selected objects since `before` as one undo step.
    void CommitTransform(const std::vector<Editor::Editing::KeyedObjectState>& before, const std::string& label);

    Editor::Editing::ObjectSelection m_selection;

    bool m_inputThisFrame = false;     // HandleWorldInput ran this frame
    bool m_panelThisFrame = false;     // RenderSelectionPanel ran this frame
    bool m_gizmoKeysForEditor = false; // last frame: the tool ran with the cursor on the world
    bool m_escapeForEditor = false;    // last frame: the tool ran with something to clear or cancel
    bool m_wasDown = false;
    bool m_dragging = false;
    bool m_dragMoved = false;
    OBJECT* m_clickedSelected = nullptr; // plain click on an already selected object
    vec3_t m_grabGround = {};
    std::vector<Editor::Editing::KeyedObjectState> m_dragStart;
    std::vector<float> m_heightAboveGround;

    CTransformGizmo m_gizmo;
    bool m_snap = false;
    std::vector<Editor::Editing::KeyedObjectState> m_gizmoStart;
    float m_gizmoPivot[3] = {};
    Editor::Editing::Transform::Delta m_gizmoDelta; // the drag as last applied (snapped)

    Field m_field = Field::None;
    std::string m_fieldLabel;
    std::vector<Editor::Editing::KeyedObjectState> m_fieldStart;

    std::string m_status;
};

#define g_MapObjectEditor CMapObjectEditor::GetInstance()

#endif // _EDITOR
