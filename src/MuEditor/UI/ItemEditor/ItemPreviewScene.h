#pragma once

#ifdef _EDITOR

#include "PreviewCharacter.h"

#include "Core/Globals/_struct.h" // ITEM_t, OBJECT
#include "Editing/PreviewCamera.h"

// What the Item Editor's 3D preview draws, through the engine's own item and
// character code, into the offscreen target that is open when Draw() runs:
//
// - Turntable: the item alone, lit like a dropped item (RenderPartObject with its
//   +level, excellent and ancient passes), long items standing upright.
// - Inventory: exactly as the inventory draws it (RenderItem3D) in a slot of the
//   item's size: per-type offset, angle and scale, hover turn.
// - Ground: as a dropped item (RenderDroppedItem) on the loaded map's ground.
// - Equipped: worn by a character (CPreviewCharacter) on that ground.
//
// The map's ground and light come from the hidden hero's spot (the studio's
// start point), or from the hero's spot in a game session.
namespace Editor::ItemEditor
{
enum class PreviewView
{
    Turntable,
    Inventory,
    Ground,
    Equipped,
};

struct PreviewSubject
{
    int itemType = -1; // group * 512 + index; -1: nothing
    PreviewView view = PreviewView::Turntable;
    ItemLook look;
    CLASS_TYPE characterClass = CLASS_KNIGHT;
    bool safeZone = false;         // Equipped: weapons on the back, as in a town
    bool showEveryEffect = false;  // +7..+15 effects whatever the game's render-level option says

    bool operator==(const PreviewSubject&) const = default;
};

// Where the orbit camera looks for a subject: a sphere around it, and the side to
// look from first.
struct SubjectFrame
{
    Gizmo::Vec3 center;
    float radius = 0.0f;
    Gizmo::Vec3 towardCamera;
};

class CItemPreviewScene
{
public:
    CItemPreviewScene() = default;
    CItemPreviewScene(const CItemPreviewScene&) = delete;
    CItemPreviewScene& operator=(const CItemPreviewScene&) = delete;

    // Sets the engine objects up for `subject` and returns where the camera should
    // look. Call when the subject changes, inside the frame (it animates models).
    SubjectFrame Prepare(const PreviewSubject& subject);

    // Draws the prepared subject into the open offscreen capture of `targetSize`
    // pixels square. `camera` is used by every view but Inventory, which has the
    // game's fixed item camera; `pointerInSlot` turns the inventory item as the
    // game does under the mouse.
    void Draw(const PreviewSubject& subject, const Preview::View& camera, int targetSize, bool pointerInSlot);

    // Takes the character's equipment off (cloth, effects).
    void Release();

    // How the last prepared Equipped subject wears its item.
    Preview::Wearing Wearing() const { return m_wearing; }

private:
    SubjectFrame PrepareTurntable(const PreviewSubject& subject);
    SubjectFrame PrepareGround(const PreviewSubject& subject);
    SubjectFrame PrepareEquipped(const PreviewSubject& subject);

    void DrawTurntable(const PreviewSubject& subject);
    void DrawInventory(const PreviewSubject& subject, int targetSize, bool pointerInSlot);
    void DrawGround();
    void DrawEquipped();

    OBJECT m_item;          // the turntable's item
    Gizmo::Vec3 m_itemAngles; // upright
    ITEM_t m_droppedItem{}; // the ground view's item
    CPreviewCharacter m_character;
    Preview::Wearing m_wearing = Preview::Wearing::NotWearable;
};
} // namespace Editor::ItemEditor

#endif // _EDITOR
