#pragma once

#ifdef _EDITOR

#include "ItemPreviewScene.h"

#include "Assets/ItemBrowse.h"
#include "Editing/PreviewCamera.h"
#include "Editing/PreviewOutfit.h"

#include "imgui.h"

#include <cstdint>
#include <optional>

// The live 3D preview in the Item Editor's Browse details: the selected item drawn
// by the engine (see ItemPreviewScene.h for the four views) into a render target
// the panel shows, with its controls (view, orbit, +level, excellent, ancient,
// the character's class).
//
// Widget code (Render) runs after the frame's scene, so it only records what to
// draw; RenderPending(), called from the scene (Core/ItemStudio), draws it into
// the target the next frame, which the panel then shows. The target is made once
// (again only when the panel's size steps past TARGET_STEP) and freed a few frames
// after the preview is no longer shown.
class CItemPreview
{
public:
    static CItemPreview& GetInstance();

    // Draws the preview of `row` and its controls. `filterClass`/`filterStage`
    // are the Browse class filter (Editor::Items::ANY_CLASS: none).
    void Render(const Editor::Items::BrowseRow& row, int filterClass, int filterStage);

    // Draws the picture the last Render() asked for. Must run inside the renderer's
    // frame (BeginFrame/EndFrame) - see Core/ItemStudio.
    void RenderPending();

    // Frees the render target and undresses the character.
    void Release();

    // The picture of the last frame (renderer texture id, 0 = none) and its side in
    // pixels, e.g. for a clean capture of the preview.
    std::uint32_t Texture() const { return m_texture; }
    int TextureSize() const { return m_textureSize; }

    // Everything the panel's controls choose, so a script (the request captures)
    // can change them and put the owner's choice back afterwards.
    struct Settings
    {
        Editor::ItemEditor::PreviewView view = Editor::ItemEditor::PreviewView::Turntable;
        Editor::Preview::Orbit orbit;
        int level = 0;
        bool excellent = false;
        bool ancient = false;
        bool showEveryEffect = false;
        bool safeZone = false;
        bool autoTurn = true;
        std::optional<Editor::Preview::ClassChoice> characterClass; // none: the Browse class filter's
        int targetSize = 0;                                         // 0: the panel's size
    };
    Settings GetSettings() const;
    // Takes effect with the next Render(); the orbit is kept over the new subject's
    // own framing.
    void ApplySettings(const Settings& settings);

    void SetView(Editor::ItemEditor::PreviewView view);
    void SetOrbit(const Editor::Preview::Orbit& orbit); // also stops the automatic turn
    void SetLevel(int level);
    void SetExcellent(bool excellent);
    void SetAncient(bool ancient);
    void SetCharacterClass(const std::optional<Editor::Preview::ClassChoice>& choice);
    void SetTargetSize(int pixels); // the picture's side; 0: the panel's size

    // Counts setting changes; DrawnSettings() reaches SettingsVersion() once the
    // picture (Texture()) shows them.
    std::uint64_t SettingsVersion() const { return m_settingsVersion; }
    std::uint64_t DrawnSettings() const { return m_drawnVersion; }

    // The item the panel showed last (its type), -1 before the first one.
    int ShownItem() const { return m_subject.itemType; }

    // How the Equipped view wears the item (after it was drawn in that view).
    Editor::Preview::Wearing Wearing() const { return m_scene.Wearing(); }

private:
    CItemPreview() = default;

    void RenderViewChoice();
    void RenderPicture(float side);
    void RenderSlotGrid(const ImVec2& corner, float side) const;
    void HandlePictureInput();
    void RenderCameraButtons();
    void RenderLookControls(const Editor::Items::BrowseRow& row);
    void RenderEquippedControls(int filterClass);
    void UpdateSubject(const Editor::Items::BrowseRow& row, int filterClass, int filterStage);
    void PrepareIfChanged();
    void KeepPinnedOrbit();
    void Changed() { ++m_settingsVersion; }

    Editor::ItemEditor::CItemPreviewScene m_scene;
    Editor::ItemEditor::PreviewSubject m_subject;  // what Render() asks for
    Editor::ItemEditor::PreviewSubject m_prepared; // what the scene is set up for
    bool m_hasPrepared = false;
    Editor::ItemEditor::SubjectFrame m_frame;
    Editor::Preview::Orbit m_orbit;

    // Controls.
    Editor::ItemEditor::PreviewView m_view = Editor::ItemEditor::PreviewView::Turntable;
    int m_level = 0;
    bool m_excellent = false;
    bool m_ancient = false;
    bool m_showEveryEffect = false;
    bool m_safeZone = false;
    bool m_autoTurn = true;
    bool m_dragging = false;
    bool m_pointerInSlot = false;
    Editor::Preview::ClassChoice m_classChoice;
    std::optional<Editor::Preview::ClassChoice> m_classOverride;
    int m_targetOverride = 0;

    // A scripted orbit, kept over the framing of the subject it was set for.
    std::optional<Editor::Preview::Orbit> m_pinnedOrbit;
    std::uint64_t m_pinnedVersion = 0;
    std::uint64_t m_settingsVersion = 0;
    std::uint64_t m_renderedVersion = 0; // the settings the last Render() recorded
    std::uint64_t m_drawnVersion = 0;    // the settings the last drawn picture shows
    bool m_showsModel = false;
    int m_slotCellsWide = 1;
    int m_slotCellsHigh = 1;

    // Render target.
    std::uint32_t m_texture = 0;
    int m_textureSize = 0;
    int m_wantedSize = 0;
    bool m_requested = false;
    int m_framesNotShown = 0;
};

#define g_ItemPreview CItemPreview::GetInstance()

#endif // _EDITOR
