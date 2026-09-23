#include "stdafx.h"

#ifdef _EDITOR

#include "MapObjectEditor.h"

#include "MapAssetReview.h"
#include "MapEditHistory.h"
#include "MapEditorShortcuts.h"
#include "MapObjectPlace.h"

#include "Camera/CameraState.h" // g_Camera: the view the world was drawn with
#include "Core/MuEditorCore.h"
#include "Editing/ObjectEditCommand.h"
#include "Editing/ObjectTransform.h"
#include "Engine/Object/w_ObjectInfo.h" // class OBJECT
#include "UI/NewUI/NewUICommon.h"       // CNewKeyInput: the game's keyboard state

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>

using Editor::Editing::KeyedObjectState;
using Editor::Editing::ObjectEditCommand;
using Editor::Editing::ObjectState;
namespace Transform = Editor::Editing::Transform;

// The primary selected object, outlined in yellow, and every selected object, sorted
// by address, outlined in orange (ZzzObject.cpp).
extern OBJECT* g_MapEditorSelectedObject;
extern std::vector<const OBJECT*> g_MapEditorSelectedObjects;
// The object a click would pick, outlined in cyan (ZzzObject.cpp).
extern OBJECT* g_MapEditorHoveredObject;

namespace
{
constexpr float SCALE_STEP = 0.05f;
constexpr float SCALE_STEP_FAST = 0.25f;
// A duplicate lands one tile east of its original (world units).
constexpr float DUPLICATE_OFFSET = TERRAIN_SCALE;
constexpr float TRANSFORM_FIELD_WIDTH = 280.0f;
constexpr float SCALE_FIELD_WIDTH = 200.0f;

const ImVec4 HINT_COLOR(0.7f, 0.9f, 1.0f, 1.0f);
const ImVec4 IDLE_COLOR(0.7f, 0.7f, 0.7f, 1.0f);
const ImVec4 DELETE_COLOR(0.8f, 0.2f, 0.2f, 1.0f);
const ImVec4 DELETE_HOVER_COLOR(0.9f, 0.3f, 0.3f, 1.0f);

// "Move object" for one object, "Move 3 objects" for more.
std::string CountLabel(const char* verb, std::size_t count, const char* suffix = "")
{
    char text[96];
    if (count == 1)
        std::snprintf(text, sizeof(text), "%s object%s", verb, suffix);
    else
        std::snprintf(text, sizeof(text), "%s %zu objects%s", verb, count, suffix);
    return text;
}

// Shift+click, and Cmd+click on a Mac (Ctrl+click elsewhere), adds or removes.
bool ToggleModifierHeld()
{
    const ImGuiIO& io = ImGui::GetIO();
    return io.KeyShift || io.KeyCtrl;
}

float HeightAboveGround(const ObjectState& state)
{
    return state.position[2] - Editor::ObjectPlace::GroundHeightAt(state.position[0], state.position[1]);
}

// The gizmo's mode keys, as ImGui and the game's keyboard state name them.
struct GizmoKey
{
    ImGuiKey key;
    int virtualKey;
    GizmoMode mode;
};
constexpr GizmoKey GIZMO_KEYS[] = {
    {ImGuiKey_W, 'W', GizmoMode::Move},
    {ImGuiKey_E, 'E', GizmoMode::Rotate},
    {ImGuiKey_R, 'R', GizmoMode::Scale},
};

// The camera the world was drawn with this frame (the Map Editor runs after the
// world pass, see SceneManager's MainScene).
Editor::Gizmo::View CurrentView()
{
    Editor::Gizmo::View view;
    std::memcpy(view.matrix, g_Camera.Matrix, sizeof(view.matrix));
    view.perspectiveX = g_Camera.PerspectiveX;
    view.perspectiveY = g_Camera.PerspectiveY;
    view.centerX = static_cast<float>(g_Camera.ScreenCenterX);
    view.centerY = static_cast<float>(g_Camera.ScreenCenterY);
    return view;
}

std::vector<ObjectState> StatesOf(const std::vector<KeyedObjectState>& objects)
{
    std::vector<ObjectState> states;
    states.reserve(objects.size());
    for (const KeyedObjectState& object : objects)
        states.push_back(object.state);
    return states;
}

const char* TransformVerb(Transform::Kind kind)
{
    switch (kind)
    {
    case Transform::Kind::Rotate:
        return "Rotate";
    case Transform::Kind::Scale:
        return "Scale";
    default:
        return "Move";
    }
}
} // namespace

CMapObjectEditor& CMapObjectEditor::GetInstance()
{
    static CMapObjectEditor instance;
    return instance;
}

void CMapObjectEditor::HandleWorldInput(const WorldInput& input)
{
    m_inputThisFrame = true;
    HandleGizmoKeys(input);
    const bool gizmoHasMouse = UpdateGizmo(input);
    OBJECT* hovered =
        (input.overWorld && !gizmoHasMouse && !m_dragging) ? Editor::ObjectPlace::PickUnderCursor() : nullptr;
    g_MapEditorHoveredObject = hovered;

    const bool pressed = input.leftDown && !m_wasDown;
    if (pressed && hovered != nullptr)
        PressOnObject(hovered, input);
    if (m_dragging && input.leftDown)
        UpdateGroundDrag(input);
    else if (m_dragging)
        EndGroundDrag();
    m_wasDown = input.leftDown;

    m_gizmoKeysForEditor = input.overWorld && !ImGui::GetIO().WantCaptureMouse;
    m_escapeForEditor = !m_selection.IsEmpty() || IsEditInProgress();
}

void CMapObjectEditor::HandleGizmoKeys(const WorldInput& input)
{
    if (Editor::Shortcuts::EscapePressed())
    {
        if (m_gizmo.IsDragging())
            CancelGizmoDrag();
        else if (m_dragging)
            CancelGroundDrag();
        else
            m_selection.Clear();
    }
    if (!input.overWorld || Editor::Shortcuts::IsTypingText())
        return;
    for (const GizmoKey& key : GIZMO_KEYS)
    {
        if (ImGui::IsKeyChordPressed(key.key))
            m_gizmo.SetMode(key.mode);
    }
}

void CMapObjectEditor::WithholdKeysFromGame()
{
    if (Editor::Shortcuts::IsTypingText())
        return;
    // Reported as held from before, the key never reads as pressed to the game.
    SEASON3B::CNewKeyInput* gameKeys = SEASON3B::CNewKeyInput::GetInstance();
    const bool escapeForEditor = m_escapeForEditor || Editor::Shortcuts::IsPopupOpen(); // Esc closes the popup
    if (escapeForEditor && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        gameKeys->SetKeyState(VK_ESCAPE, SEASON3B::CNewKeyInput::KEY_REPEAT);
    if (!m_gizmoKeysForEditor)
        return;
    for (const GizmoKey& key : GIZMO_KEYS)
    {
        if (ImGui::IsKeyChordPressed(key.key))
            gameKeys->SetKeyState(key.virtualKey, SEASON3B::CNewKeyInput::KEY_REPEAT);
    }
}

bool CMapObjectEditor::UpdateGizmo(const WorldInput& input)
{
    if (m_selection.IsEmpty() || m_dragging)
        return false;
    float pivot[3];
    Transform::Pivot(SelectionStates(), pivot);
    const bool mouseOverWorld = input.overWorld && !ImGui::GetIO().WantCaptureMouse;
    const GizmoFrame frame = m_gizmo.Update(CurrentView(), {pivot[0], pivot[1], pivot[2]}, mouseOverWorld);
    switch (frame.phase)
    {
    case GizmoFrame::Phase::Idle:
        return false;
    case GizmoFrame::Phase::Hovered:
        break;
    case GizmoFrame::Phase::Began:
        BeginGizmoDrag();
        break;
    case GizmoFrame::Phase::Dragging:
        ApplyGizmoDelta(frame.delta);
        m_gizmo.DrawDragLabel(m_gizmoDelta);
        break;
    case GizmoFrame::Phase::Ended:
        EndGizmoDrag(frame.delta);
        break;
    }
    // The gizmo has the mouse: no pick, no ground drag and no click for the game.
    g_MuEditorCore.SetHoveringUI(true);
    return true;
}

void CMapObjectEditor::BeginGizmoDrag()
{
    m_gizmoStart = g_MapEditHistory.Objects().Capture(m_selection.Objects());
    Transform::Pivot(StatesOf(m_gizmoStart), m_gizmoPivot);
    m_gizmoDelta = Transform::Delta{};
}

void CMapObjectEditor::ApplyGizmoDelta(const Transform::Delta& delta)
{
    if (m_gizmoStart.empty())
        return;
    // Snapping is measured on the primary, the last object picked.
    m_gizmoDelta = m_snap ? Transform::Snapped(delta, m_gizmoStart.back().state) : delta;
    ApplyToSelection(Transform::Apply(StatesOf(m_gizmoStart), m_gizmoPivot, m_gizmoDelta));
}

void CMapObjectEditor::EndGizmoDrag(const Transform::Delta& delta)
{
    ApplyGizmoDelta(delta);
    CommitTransform(m_gizmoStart, CountLabel(TransformVerb(m_gizmoDelta.kind), m_gizmoStart.size()));
    m_gizmo.EndDrag();
    m_gizmoStart.clear();
}

void CMapObjectEditor::CancelGizmoDrag()
{
    ApplyToSelection(StatesOf(m_gizmoStart));
    m_gizmo.EndDrag();
    m_gizmoStart.clear();
    m_status = "Transform cancelled.";
}

void CMapObjectEditor::ApplyTransform(const Transform::Delta& delta)
{
    if (m_selection.IsEmpty() || IsEditInProgress())
        return;
    BeginGizmoDrag();
    EndGizmoDrag(delta);
}

void CMapObjectEditor::PressOnObject(OBJECT* hit, const WorldInput& input)
{
    if (ToggleModifierHeld())
    {
        m_selection.Toggle(hit);
        return;
    }
    const bool alreadySelected = m_selection.Contains(hit);
    // A click (without a drag) on one of several selected objects selects only it.
    m_clickedSelected = (alreadySelected && m_selection.Count() > 1) ? hit : nullptr;
    if (alreadySelected)
        m_selection.Add(hit); // becomes the primary
    else
        m_selection.SelectOnly(hit);
    // Without a ground point under the cursor (it points at the sky) there is
    // nothing to drag along; the click only selects.
    if (input.groundValid)
        BeginGroundDrag(input);
}

void CMapObjectEditor::BeginGroundDrag(const WorldInput& input)
{
    m_dragging = true;
    m_dragMoved = false;
    VectorCopy(input.ground, m_grabGround);
    m_dragStart = g_MapEditHistory.Objects().Capture(m_selection.Objects());
    m_heightAboveGround.clear();
    for (const KeyedObjectState& object : m_dragStart)
        m_heightAboveGround.push_back(HeightAboveGround(object.state));
}

// Every selected object keeps its offset from the grabbed ground point and its
// height above the ground, so a group walks up and down slopes together.
void CMapObjectEditor::UpdateGroundDrag(const WorldInput& input)
{
    if (!input.groundValid || !input.overWorld)
        return;
    // A click does not move anything: the drag starts past ImGui's drag threshold.
    if (!m_dragMoved && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        return;
    m_dragMoved = true;
    const float dx = input.ground[0] - m_grabGround[0];
    const float dy = input.ground[1] - m_grabGround[1];
    std::vector<ObjectState> states;
    states.reserve(m_dragStart.size());
    for (std::size_t i = 0; i < m_dragStart.size(); ++i)
    {
        ObjectState state = m_dragStart[i].state;
        state.position[0] += dx;
        state.position[1] += dy;
        state.position[2] =
            Editor::ObjectPlace::GroundHeightAt(state.position[0], state.position[1]) + m_heightAboveGround[i];
        states.push_back(state);
    }
    ApplyToSelection(states);
}

void CMapObjectEditor::EndGroundDrag()
{
    m_dragging = false;
    if (m_dragMoved)
        CommitTransform(m_dragStart, CountLabel("Move", m_dragStart.size()));
    else if (m_clickedSelected != nullptr)
        m_selection.SelectOnly(m_clickedSelected);
    m_clickedSelected = nullptr;
    m_dragStart.clear();
}

void CMapObjectEditor::CancelGroundDrag()
{
    std::vector<ObjectState> states;
    for (const KeyedObjectState& object : m_dragStart)
        states.push_back(object.state);
    ApplyToSelection(states);
    m_dragging = false;
    m_clickedSelected = nullptr;
    m_dragStart.clear();
    m_status = "Move cancelled.";
}

void CMapObjectEditor::ApplyToSelection(const std::vector<ObjectState>& states)
{
    const std::vector<OBJECT*> objects = m_selection.Objects();
    const std::size_t count = std::min(objects.size(), states.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        OBJECT* applied = g_MapEditHistory.Objects().Apply(objects[i], states[i]);
        m_selection.Replace(objects[i], applied);
    }
}

std::vector<ObjectState> CMapObjectEditor::SelectionStates() const
{
    std::vector<ObjectState> states;
    states.reserve(m_selection.Count());
    for (const OBJECT* object : m_selection.Objects())
        states.push_back(CMapObjectWorld::StateOf(object));
    return states;
}

void CMapObjectEditor::CommitTransform(const std::vector<KeyedObjectState>& before, const std::string& label)
{
    CMapObjectWorld& world = g_MapEditHistory.Objects();
    std::vector<Editor::Editing::ObjectChange> changes =
        Editor::Editing::TransformChanges(before, world.CaptureAgain(before));
    if (changes.empty())
        return;
    g_MapEditHistory.Push(std::make_unique<ObjectEditCommand>(label, world, std::move(changes)));
    m_status = label + ".";
}

void CMapObjectEditor::RenderSelectionPanel(int world, bool& showInAssetsTab)
{
    m_panelThisFrame = true;
    ImGui::TextColored(HINT_COLOR, "Click an object to select it, Shift/Cmd+click to add or remove one.");
    ImGui::TextColored(HINT_COLOR, "Drag an object to move the selection along the ground, or use the gizmo.");
    ImGui::TextColored(HINT_COLOR, "W/E/R: move, rotate, scale (cursor over the world). Esc clears or cancels.");
    if (m_selection.IsEmpty())
    {
        ImGui::TextColored(IDLE_COLOR, "(nothing selected)");
        return;
    }

    OBJECT* primary = Primary();
    if (m_selection.Count() == 1)
        ImGui::Text("Selected: type %d", primary->Type);
    else
        ImGui::Text("Selected: %zu objects. The fields show the last one picked (type %d); an edit changes all.",
                    m_selection.Count(), primary->Type);
    if (g_MapAssetReview.RenderObjectSummary(world, primary))
        showInAssetsTab = true;

    RenderTransformFields();
    RenderGizmoControls();
    RenderActions();
}

void CMapObjectEditor::RenderGizmoControls()
{
    ImGui::TextUnformatted("Gizmo:");
    struct ModeButton
    {
        const char* label;
        GizmoMode mode;
    };
    constexpr ModeButton MODE_BUTTONS[] = {
        {"Move (W)", GizmoMode::Move}, {"Rotate (E)", GizmoMode::Rotate}, {"Scale (R)", GizmoMode::Scale}};
    for (const ModeButton& button : MODE_BUTTONS)
    {
        ImGui::SameLine();
        if (ImGui::RadioButton(button.label, m_gizmo.Mode() == button.mode))
            m_gizmo.SetMode(button.mode);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &m_snap);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Move in quarter tiles (%.0f units), turn in %.0f degree steps, scale in steps of %.1f.",
                          Transform::MOVE_SNAP, Transform::ROTATE_SNAP_DEGREES, Transform::SCALE_SNAP);
}

void CMapObjectEditor::RenderTransformFields()
{
    const ObjectState primary = CMapObjectWorld::StateOf(Primary());

    float position[3] = {primary.position[0], primary.position[1], primary.position[2]};
    ImGui::SetNextItemWidth(TRANSFORM_FIELD_WIDTH);
    const bool positionChanged = ImGui::InputFloat3("Pos", position, "%.0f");
    HandleFieldWidget(Field::Position, "Edit position", positionChanged,
                      [&](ObjectState& state)
                      {
                          for (int k = 0; k < 3; ++k)
                              state.position[k] += position[k] - primary.position[k];
                      });

    float angle[3] = {primary.angle[0], primary.angle[1], primary.angle[2]};
    ImGui::SetNextItemWidth(TRANSFORM_FIELD_WIDTH);
    const bool angleChanged = ImGui::InputFloat3("Angle", angle, "%.0f");
    HandleFieldWidget(Field::Angle, "Edit angle", angleChanged,
                      [&](ObjectState& state)
                      {
                          for (int k = 0; k < 3; ++k)
                              state.angle[k] += angle[k] - primary.angle[k];
                      });

    float scale = primary.scale;
    ImGui::SetNextItemWidth(SCALE_FIELD_WIDTH);
    const bool scaleChanged = ImGui::InputFloat("Scale ", &scale, SCALE_STEP, SCALE_STEP_FAST, "%.2f");
    HandleFieldWidget(Field::Scale, "Edit scale", scaleChanged, [&](ObjectState& state)
                      { state.scale = std::max(Transform::MIN_OBJECT_SCALE, state.scale + (scale - primary.scale)); });
}

void CMapObjectEditor::HandleFieldWidget(Field field, const char* label, bool changed,
                                         const std::function<void(ObjectState&)>& change)
{
    const bool active = ImGui::IsItemActive();
    if (active || changed)
        BeginFieldEdit(field, label);
    if (changed)
    {
        std::vector<ObjectState> states = SelectionStates();
        for (ObjectState& state : states)
            change(state);
        ApplyToSelection(states);
    }
    if (!active)
        EndFieldEdit(field);
}

void CMapObjectEditor::BeginFieldEdit(Field field, const char* label)
{
    if (m_field == field)
        return;
    CommitFieldEdit();
    m_field = field;
    m_fieldLabel = label;
    m_fieldStart = g_MapEditHistory.Objects().Capture(m_selection.Objects());
}

void CMapObjectEditor::EndFieldEdit(Field field)
{
    if (m_field == field)
        CommitFieldEdit();
}

void CMapObjectEditor::CommitFieldEdit()
{
    if (m_field == Field::None)
        return;
    CommitTransform(m_fieldStart, m_fieldLabel);
    m_field = Field::None;
    m_fieldStart.clear();
}

void CMapObjectEditor::RenderActions()
{
    if (ImGui::Button("Drop to ground"))
        DropSelectionToGround();
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") || Editor::Shortcuts::DuplicatePressed())
        DuplicateSelection();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Copies of the selected objects, one tile east (%s+D)",
                          ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd" : "Ctrl");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, DELETE_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DELETE_HOVER_COLOR);
    const bool deleteClicked = ImGui::Button(m_selection.Count() > 1 ? "Delete objects" : "Delete object");
    ImGui::PopStyleColor(2);
    if (deleteClicked || Editor::Shortcuts::DeletePressed())
        DeleteSelection();
}

void CMapObjectEditor::DropSelectionToGround()
{
    if (m_selection.IsEmpty() || IsEditInProgress())
        return;
    const std::vector<KeyedObjectState> before = g_MapEditHistory.Objects().Capture(m_selection.Objects());
    std::vector<ObjectState> states = SelectionStates();
    for (ObjectState& state : states)
        state.position[2] = Editor::ObjectPlace::GroundHeightAt(state.position[0], state.position[1]);
    ApplyToSelection(states);
    CommitTransform(before, CountLabel("Drop", before.size(), " to ground"));
}

void CMapObjectEditor::DuplicateSelection()
{
    if (m_selection.IsEmpty() || IsEditInProgress())
        return;
    CMapObjectWorld& world = g_MapEditHistory.Objects();
    std::vector<KeyedObjectState> created;
    std::vector<OBJECT*> copies;
    for (const OBJECT* original : m_selection.Objects())
    {
        ObjectState state = CMapObjectWorld::StateOf(original);
        const float heightAboveGround = HeightAboveGround(state);
        state.position[0] += DUPLICATE_OFFSET;
        state.position[2] =
            Editor::ObjectPlace::GroundHeightAt(state.position[0], state.position[1]) + heightAboveGround;
        OBJECT* copy = world.CreateNew(state);
        if (copy == nullptr)
            continue; // one tile further is off the map
        created.push_back({copy->SaveOrder, CMapObjectWorld::StateOf(copy)});
        copies.push_back(copy);
    }
    if (created.empty())
    {
        m_status = "Nothing duplicated: one tile east is off the map.";
        return;
    }
    m_selection.Assign(copies);
    const std::string label = CountLabel("Duplicate", created.size());
    g_MapEditHistory.Push(std::make_unique<ObjectEditCommand>(label, world, Editor::Editing::CreationChanges(created)));
    m_status = label + ".";
}

void CMapObjectEditor::DeleteSelection()
{
    if (m_selection.IsEmpty() || IsEditInProgress())
        return;
    CMapObjectWorld& world = g_MapEditHistory.Objects();
    const std::vector<KeyedObjectState> removed = world.Capture(m_selection.Objects());
    for (OBJECT* object : m_selection.Objects())
        Editor::ObjectPlace::Remove(object);
    m_selection.Clear();
    const std::string label = CountLabel("Delete", removed.size());
    g_MapEditHistory.Push(std::make_unique<ObjectEditCommand>(label, world, Editor::Editing::RemovalChanges(removed)));
    m_status = label + ".";
}

void CMapObjectEditor::Select(const std::vector<OBJECT*>& objects)
{
    if (m_dragging || m_gizmo.IsDragging())
        return;
    CommitFieldEdit();
    m_selection.Assign(objects);
}

void CMapObjectEditor::Toggle(OBJECT* object)
{
    if (m_dragging || m_gizmo.IsDragging())
        return;
    CommitFieldEdit();
    m_selection.Toggle(object);
}

bool CMapObjectEditor::IsEditInProgress() const
{
    return m_dragging || m_gizmo.IsDragging() || m_field != Field::None;
}

void CMapObjectEditor::EndFrame()
{
    if (!m_inputThisFrame)
    {
        if (m_dragging)
            EndGroundDrag();
        if (m_gizmo.IsDragging())
            EndGizmoDrag(m_gizmoDelta);
        m_wasDown = false;
        m_gizmoKeysForEditor = false;
        m_escapeForEditor = false;
    }
    if (!m_panelThisFrame)
        CommitFieldEdit();
    m_inputThisFrame = false;
    m_panelThisFrame = false;
}

void CMapObjectEditor::OnHistoryStep(bool undone)
{
    TouchedObjects touched = g_MapEditHistory.Objects().TakeTouched();
    if (!touched.any)
        return;
    if (undone)
        std::reverse(touched.live.begin(), touched.live.end());
    m_selection.Assign(touched.live);
    m_status.clear(); // the last action's line no longer describes the objects
}

void CMapObjectEditor::Forget()
{
    m_selection.Clear();
    m_wasDown = false;
    m_dragging = false;
    m_dragMoved = false;
    m_clickedSelected = nullptr;
    m_dragStart.clear();
    m_field = Field::None;
    m_fieldStart.clear();
    m_gizmo.EndDrag();
    m_gizmoStart.clear();
}

void CMapObjectEditor::PublishOutline(bool show)
{
    if (!show || m_selection.IsEmpty())
    {
        g_MapEditorSelectedObject = nullptr;
        g_MapEditorSelectedObjects.clear();
        return;
    }
    g_MapEditorSelectedObject = Primary();
    const std::vector<OBJECT*>& objects = m_selection.Objects();
    g_MapEditorSelectedObjects.assign(objects.begin(), objects.end());
    std::sort(g_MapEditorSelectedObjects.begin(), g_MapEditorSelectedObjects.end());
}

#endif // _EDITOR
