#pragma once

#ifdef _EDITOR

#include "ItemPreviewScene.h"

#include "Assets/ItemBrowse.h"
#include "Editing/PreviewCamera.h"
#include "Editing/PreviewOutfit.h"

#include "imgui.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Editor::Assets::HotReload
{
class ModelCopy;
}

// A second picture next to the first (the Item Editor's A/B compare): the same
// item drawn from other files, in the same view, camera, +level and options.
struct PreviewCompare
{
    // Not owned; each is swapped into Models[] while the right picture is drawn
    // (HotReload::ScopedModelSwap). The owner retires them, never frees them at once.
    std::vector<Editor::Assets::HotReload::ModelCopy*> copies;
    std::string leftLabel;
    std::string rightLabel;
    std::string rightMessage;     // without copies: why the right picture is empty
    std::uint64_t generation = 0; // changes whenever the copies change
};

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

    // Asks the next Render() for two pictures side by side (the right one from
    // `compare`'s copies), with one camera: dragging either turns both. Without a
    // call before a Render() the preview shows one picture.
    void SetCompare(PreviewCompare compare);

    // Draws the picture the last Render() asked for. Must run inside the renderer's
    // frame (BeginFrame/EndFrame) - see Core/ItemStudio.
    void RenderPending();

    // Frees the render target and undresses the character.
    void Release();

    // A model the preview may show was loaded again (hot reload): both pictures are
    // framed and dressed again with the next draw; the targets and the camera stay.
    // Safe to call from widget code, unlike Release(), whose targets this frame's
    // ImGui draw may still show.
    void ModelsChanged();

    // The picture of the last frame (renderer texture id, 0 = none) and its side in
    // pixels, e.g. for a clean capture of the preview.
    std::uint32_t Texture() const { return m_texture; }
    int TextureSize() const { return m_textureSize; }
    // The right picture of the side-by-side view (0 = none), drawn in the same
    // frame and with the same settings as Texture().
    std::uint32_t CompareTexture() const { return m_compareTexture; }
    int CompareTextureSize() const { return m_compareTextureSize; }

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

    // Mouse over the pictures in one frame.
    struct PictureInput
    {
        bool hovered = false;
        bool dragging = false;
    };

    void RenderViewChoice();
    void RenderPictures(float side);
    void RenderPicture(float side, std::uint32_t texture, int textureSize, const char* id, const char* label,
                       PictureInput& input, const char* message = nullptr);
    void RenderSlotGrid(const ImVec2& corner, float side) const;
    void ApplyPictureInput(const PictureInput& input);
    void RenderCameraButtons();
    void RenderLookControls(const Editor::Items::BrowseRow& row);
    void RenderEquippedControls(int filterClass);
    void UpdateSubject(const Editor::Items::BrowseRow& row, int filterClass, int filterStage);
    void PrepareIfChanged();
    bool DrawsCompare() const { return m_compare && !m_compare->copies.empty(); }
    void PrepareCompareIfChanged();
    Editor::ItemEditor::SubjectFrame SharedFrame() const;
    bool DrawPicture(Editor::ItemEditor::CItemPreviewScene& scene, std::uint32_t& texture, int& textureSize,
                     const Editor::ItemEditor::SubjectFrame& frame);
    void DrawComparePicture(const Editor::ItemEditor::SubjectFrame& frame);
    void ReleaseCompare();
    void KeepPinnedOrbit();
    void Changed() { ++m_settingsVersion; }

    Editor::ItemEditor::CItemPreviewScene m_scene;
    Editor::ItemEditor::PreviewSubject m_subject;  // what Render() asks for
    Editor::ItemEditor::PreviewSubject m_prepared; // what the scene is set up for
    bool m_hasPrepared = false;
    bool m_modelsChanged = false;        // prepare again, keep the orbit
    bool m_compareModelsChanged = false; // the same for the right picture
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

    // Side by side: the right picture's scene, frame and target.
    std::optional<PreviewCompare> m_nextCompare; // for the next Render()
    std::optional<PreviewCompare> m_compare;     // what the last Render() showed
    Editor::ItemEditor::CItemPreviewScene m_compareScene;
    Editor::ItemEditor::PreviewSubject m_comparePrepared;
    std::uint64_t m_compareGeneration = 0;
    bool m_hasComparePrepared = false;
    Editor::ItemEditor::SubjectFrame m_compareFrame;
    std::uint32_t m_compareTexture = 0;
    int m_compareTextureSize = 0;
    int m_framesWithoutCompare = 0;
};

#define g_ItemPreview CItemPreview::GetInstance()

#endif // _EDITOR
