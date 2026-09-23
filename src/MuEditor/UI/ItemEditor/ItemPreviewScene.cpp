#include "stdafx.h"

#ifdef _EDITOR

#include "ItemPreviewScene.h"

#include "Core/ItemStudio.h"
#include "Core/ModelPose.h"
#include "Editing/PreviewSlot.h"
#include "Editing/ThumbnailFraming.h"

#include "Camera/CameraState.h"
#include "Character/CharacterManager.h"  // GetEquipedBowType
#include "Engine/Object/ZzzCharacter.h"   // Hero
#include "Engine/Object/ZzzInfomation.h"  // ItemAttribute
#include "Engine/Object/ZzzInventory.h"   // RenderItem3D
#include "Engine/Object/ZzzObject.h"      // ItemObjectAttribute, PlaceItemOnGround, RenderDroppedItem
#include "Render/Effects/ZzzEffect.h"     // sprites and particles
#include "Render/Models/ZzzBMD.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/NewUI/Inventory/NewUIInventoryCtrl.h"
#include "UI/NewUI/NewUISystem.h" // g_pOption

#include <algorithm>
#include <cmath>
#include <cstring>

extern float BoneScale;
extern int TerrainFlag;
extern int MouseX;
extern int MouseY;
extern vec3_t MousePosition;

namespace Editor::ItemEditor
{
namespace
{
using Gizmo::Vec3;

// The inventory's 3D camera (CNewUI3DCamera::Render: gluPerspective2(1.f, ...)).
constexpr float INVENTORY_FOV_DEGREES = 1.0f;
// The option window's highest render level: every +7..+15 effect.
constexpr int EVERY_EFFECT_RENDER_LEVEL = 4;
// A mouse position the inventory slot never contains / always contains.
constexpr int POINTER_OUTSIDE = -10000;
constexpr int POINTER_INSIDE_OFFSET = 1;

// The ground drawn under the dropped item and the character: this many map tiles
// on each side of the spot.
constexpr int GROUND_TILES_AROUND = 5;
// Ground view: room around a dropped item, and at least this much around it.
constexpr float GROUND_ROOM = 1.5f;
constexpr float GROUND_MIN_RADIUS = 80.0f;
const Vec3 GROUND_VIEW_FROM{-0.4f, -1.0f, 1.0f}; // from above, like the game's camera

// Equipped view: the character's middle and the sphere its wings stay in.
constexpr float CHARACTER_MIDDLE_HEIGHT = 110.0f;
constexpr float CHARACTER_RADIUS = 150.0f;
// A held item reaches this far from the character's middle plus its own length;
// wings spread about this many times their own radius from it.
constexpr float HAND_REACH = 60.0f;
constexpr float WING_ROOM = 1.5f;
const Vec3 CHARACTER_FRONT{0.0f, -1.0f, 0.2f}; // a character at angle 0 faces -Y

constexpr float FALLBACK_ITEM_RADIUS = 50.0f;

Vec3 ToVec(const vec3_t v)
{
    return {v[0], v[1], v[2]};
}

bool IsArmourModel(int modelType)
{
    return modelType >= MODEL_HELM && modelType < MODEL_BOOTS + MAX_ITEM_INDEX;
}

bool IsLoaded(int modelType)
{
    return modelType >= 0 && modelType < MAX_MODELS && ::Models[modelType].NumMeshs > 0;
}

// The spot the previews stand on: the hidden hero's (the studio's start point),
// or the hero's in a game session, on the map's ground.
void StudioSpot(vec3_t spot)
{
    const float x = Hero->Object.Position[0];
    const float y = Hero->Object.Position[1];
    Vector(x, y, RequestTerrainHeight(x, y), spot);
}

// Everything the preview changes for a draw, put back when it ends: the camera the
// engine projects with, the inventory's screen mapping, the mouse the inventory
// checks, the render-level option and the matrices.
class ScopedSceneState
{
public:
    explicit ScopedSceneState(bool showEveryEffect)
        : m_camera(g_Camera), m_rateX(g_fScreenRate_x), m_rateY(g_fScreenRate_y), m_offsetX(g_fScreenOffset_x),
          m_offsetY(g_fScreenOffset_y), m_mouseX(MouseX), m_mouseY(MouseY), m_terrainFlag(TerrainFlag),
          m_renderLevel(g_pOption->GetRenderLevel())
    {
        VectorCopy(MousePosition, m_mousePosition);
        mu::GetRenderer().SetMatrixMode(GL_PROJECTION);
        mu::GetRenderer().PushMatrix();
        mu::GetRenderer().SetMatrixMode(GL_MODELVIEW);
        mu::GetRenderer().PushMatrix();
        if (showEveryEffect)
            g_pOption->SetRenderLevel(EVERY_EFFECT_RENDER_LEVEL);
    }

    ~ScopedSceneState()
    {
        g_pOption->SetRenderLevel(m_renderLevel);
        mu::GetRenderer().SetMatrixMode(GL_PROJECTION);
        mu::GetRenderer().PopMatrix();
        mu::GetRenderer().SetMatrixMode(GL_MODELVIEW);
        mu::GetRenderer().PopMatrix();
        g_Camera = m_camera;
        g_fScreenRate_x = m_rateX;
        g_fScreenRate_y = m_rateY;
        g_fScreenOffset_x = m_offsetX;
        g_fScreenOffset_y = m_offsetY;
        MouseX = m_mouseX;
        MouseY = m_mouseY;
        TerrainFlag = m_terrainFlag;
        VectorCopy(m_mousePosition, MousePosition);
        BoneScale = 1.0f;
    }

    ScopedSceneState(const ScopedSceneState&) = delete;
    ScopedSceneState& operator=(const ScopedSceneState&) = delete;

private:
    CameraState m_camera;
    float m_rateX;
    float m_rateY;
    float m_offsetX;
    float m_offsetY;
    int m_mouseX;
    int m_mouseY;
    int m_terrainFlag;
    int m_renderLevel;
    vec3_t m_mousePosition;
};

void BeginDrawing()
{
    EnableDepthTest();
    EnableDepthMask();
    DisableAlphaBlend();
}

// The orbit camera, and the engine's camera matrix to match (sprites are placed
// with it).
void LoadCamera(const Preview::View& view)
{
    mu::GetRenderer().SetMatrixMode(GL_PROJECTION);
    mu::GetRenderer().LoadIdentity();
    gluPerspective(view.fovDegrees, 1.0f, view.zNear, view.zFar);
    mu::GetRenderer().SetMatrixMode(GL_MODELVIEW);
    float matrix[16];
    Preview::LookAtColumnMajor(view, matrix);
    mu::GetRenderer().LoadMatrix(matrix);
    Preview::LookAtRows(view, g_Camera.Matrix);
    Vector(view.eye.x, view.eye.y, view.eye.z, g_Camera.Position);
}

// Poses the model an item is drawn with (armour parts use the character's
// skeleton, as on the ground and in the inventory) into BoneTransform.
void PoseItem(int modelType, const Vec3& angles)
{
    const int skeletonType = IsArmourModel(modelType) ? MODEL_PLAYER : modelType;
    BMD& skeleton = ::Models[skeletonType];
    skeleton.BodyHeight = 0.0f;
    skeleton.CurrentAction = 0;
    BoneScale = 1.0f;
    vec3_t angle = {angles.x, angles.y, angles.z};
    vec3_t headAngle = {0.0f, 0.0f, 0.0f};
    skeleton.Animation(BoneTransform, 0.0f, 0.0f, 0, angle, headAngle, false, false);
}

// The item's bounds in model units in the pose PoseItem set; false without vertices.
bool PosedBounds(int modelType, Vec3& boundsMin, Vec3& boundsMax)
{
    vec3_t low;
    vec3_t high;
    if (!ModelPose::Bounds(::Models[modelType], low, high))
        return false;
    boundsMin = ToVec(low);
    boundsMax = ToVec(high);
    return true;
}

// The sphere the dressed character and what it holds or wears on the back stay in;
// `itemRadius` is the item's own (as on the turntable).
float EquippedRadius(Preview::Wearing wearing, float itemRadius)
{
    switch (wearing)
    {
    case Preview::Wearing::Armour:
    case Preview::Wearing::NotShown:
    case Preview::Wearing::NotWearable:
        return CHARACTER_RADIUS;
    case Preview::Wearing::Wings:
        return std::max(CHARACTER_RADIUS, WING_ROOM * itemRadius);
    default:
        return std::max(CHARACTER_RADIUS, HAND_REACH + 2.0f * itemRadius);
    }
}

// The ground of the loaded map around `spot`, drawn by the terrain's own tiles.
void DrawGroundPatch(const vec3_t spot)
{
    const int centerX = static_cast<int>(spot[0] / TERRAIN_SCALE);
    const int centerY = static_cast<int>(spot[1] / TERRAIN_SCALE);
    const int first = 0;
    const int last = TERRAIN_SIZE - 2; // a tile reads its corner at +1
    TerrainFlag = TERRAIN_MAP_NORMAL;
    DisableAlphaBlend();
    for (int y = std::max(first, centerY - GROUND_TILES_AROUND); y <= std::min(last, centerY + GROUND_TILES_AROUND); ++y)
    {
        for (int x = std::max(first, centerX - GROUND_TILES_AROUND); x <= std::min(last, centerX + GROUND_TILES_AROUND);
             ++x)
        {
            RenderTerrainTile(static_cast<float>(x), static_cast<float>(y), x, y, 1.0f, 1, false);
        }
    }
    DisableAlphaBlend();
}

// Sprites the character's items made this frame (weapon glows), and in the studio,
// where nothing else makes any, the particles too.
void DrawEffects()
{
    CheckSprites();
    BeginSprite();
    RenderSprites();
    if (Editor::ItemStudio::ShowsBackdropOnly())
        RenderParticles();
    EndSprite();
}
} // namespace

SubjectFrame CItemPreviewScene::Prepare(const PreviewSubject& subject)
{
    switch (subject.view)
    {
    case PreviewView::Ground:
        return PrepareGround(subject);
    case PreviewView::Equipped:
        return PrepareEquipped(subject);
    case PreviewView::Turntable:
    case PreviewView::Inventory:
        break;
    }
    return PrepareTurntable(subject);
}

SubjectFrame CItemPreviewScene::PrepareTurntable(const PreviewSubject& subject)
{
    m_item.Type = MODEL_ITEM + subject.itemType;
    ItemObjectAttribute(&m_item);
    StudioSpot(m_item.Position);
    m_itemAngles = {};

    SubjectFrame frame{ToVec(m_item.Position), FALLBACK_ITEM_RADIUS, CHARACTER_FRONT};
    if (!IsLoaded(m_item.Type))
        return frame;

    Vec3 boundsMin;
    Vec3 boundsMax;
    PoseItem(m_item.Type, m_itemAngles);
    if (!PosedBounds(m_item.Type, boundsMin, boundsMax))
        return frame;
    m_itemAngles = Preview::UprightAngles(boundsMin, boundsMax);
    PoseItem(m_item.Type, m_itemAngles);
    PosedBounds(m_item.Type, boundsMin, boundsMax);

    const Vec3 middle = (boundsMin + boundsMax) * 0.5f;
    frame.center = ToVec(m_item.Position) + middle * m_item.Scale;
    frame.radius = 0.5f * Gizmo::Length(boundsMax - boundsMin) * m_item.Scale;
    const Thumbnail::Camera broadSide = Thumbnail::FrameBounds(boundsMin, boundsMax, Thumbnail::Framing::Item);
    frame.towardCamera = broadSide.eye - broadSide.center;
    return frame;
}

SubjectFrame CItemPreviewScene::PrepareGround(const PreviewSubject& subject)
{
    const SubjectFrame alone = PrepareTurntable(subject);

    ITEM& item = m_droppedItem.Item;
    item.Type = subject.itemType;
    item.Level = subject.look.level;
    item.ExcellentFlags = static_cast<BYTE>(subject.look.excellentFlags);
    item.AncientDiscriminator = static_cast<BYTE>(subject.look.ancientDiscriminator);

    OBJECT& object = m_droppedItem.Object;
    object.Type = MODEL_ITEM + subject.itemType;
    ItemObjectAttribute(&object);
    StudioSpot(object.Position);
    PlaceItemOnGround(&object);

    vec3_t ground;
    StudioSpot(ground);
    return {ToVec(ground), std::max(GROUND_MIN_RADIUS, alone.radius * GROUND_ROOM), GROUND_VIEW_FROM};
}

SubjectFrame CItemPreviewScene::PrepareEquipped(const PreviewSubject& subject)
{
    const ITEM_ATTRIBUTE& attribute = ItemAttribute[subject.itemType];
    ITEM probe{};
    probe.Type = subject.itemType;
    const int bowType = gCharacterManager.GetEquipedBowType(&probe);
    const Preview::BowKind bowKind = bowType == BOWTYPE_BOW        ? Preview::BowKind::Bow
                                     : bowType == BOWTYPE_CROSSBOW ? Preview::BowKind::Crossbow
                                                                   : Preview::BowKind::None;
    const auto exists = [](int itemType) {
        return ItemAttribute[itemType].Name[0] != 0 && IsLoaded(MODEL_ITEM + itemType);
    };
    const Preview::Outfit outfit = Preview::DressFor(subject.itemType, attribute.m_byItemSlot, bowKind, exists);
    m_wearing = outfit.wearing;

    vec3_t spot;
    StudioSpot(spot);
    m_character.Dress(outfit, subject.characterClass, subject.look, spot, subject.safeZone);
    const Vec3 middle = ToVec(spot) + Vec3{0.0f, 0.0f, CHARACTER_MIDDLE_HEIGHT};
    return {middle, EquippedRadius(outfit.wearing, PrepareTurntable(subject).radius), CHARACTER_FRONT};
}

void CItemPreviewScene::Draw(const PreviewSubject& subject, const Preview::View& camera, int targetSize,
                             bool pointerInSlot)
{
    ScopedSceneState state(subject.showEveryEffect);
    BeginDrawing();
    if (subject.view == PreviewView::Inventory)
    {
        DrawInventory(subject, targetSize, pointerInSlot);
        return;
    }

    LoadCamera(camera);
    switch (subject.view)
    {
    case PreviewView::Ground:
        DrawGround();
        break;
    case PreviewView::Equipped:
        DrawEquipped();
        break;
    case PreviewView::Turntable:
    case PreviewView::Inventory:
        DrawTurntable(subject);
        break;
    }
}

void CItemPreviewScene::DrawTurntable(const PreviewSubject& subject)
{
    if (!IsLoaded(m_item.Type))
        return;
    PoseItem(m_item.Type, m_itemAngles);
    vec3_t light;
    RequestTerrainLight(m_item.Position[0], m_item.Position[1], light);
    VectorAdd(light, m_item.Light, light);
    RenderPartObject(&m_item, m_item.Type, nullptr, light, m_item.Alpha, subject.look.level,
                     subject.look.excellentFlags, subject.look.ancientDiscriminator, true, true, true);
}

void CItemPreviewScene::DrawInventory(const PreviewSubject& subject, int targetSize, bool pointerInSlot)
{
    const ITEM_ATTRIBUTE& attribute = ItemAttribute[subject.itemType];
    const Preview::InventoryMetrics metrics{static_cast<float>(SEASON3B::INVENTORY_SQUARE_WIDTH),
                                            static_cast<float>(REFERENCE_HEIGHT), INVENTORY_FOV_DEGREES};
    const Preview::SlotProjection slot =
        Preview::SlotInTarget(targetSize, targetSize, attribute.Width, attribute.Height, metrics);

    mu::GetRenderer().SetMatrixMode(GL_PROJECTION);
    mu::GetRenderer().LoadIdentity();
    gluPerspective(slot.fovDegrees, 1.0f, RENDER_ITEMVIEW_NEAR, RENDER_ITEMVIEW_FAR);
    mu::GetRenderer().SetMatrixMode(GL_MODELVIEW);
    mu::GetRenderer().LoadIdentity();

    // The inventory's camera: at the origin, looking down -Z (CNewUI3DCamera::Render).
    std::memset(g_Camera.Matrix, 0, sizeof(g_Camera.Matrix));
    g_Camera.Matrix[0][0] = g_Camera.Matrix[1][1] = g_Camera.Matrix[2][2] = 1.0f;
    g_Camera.ScreenCenterX = slot.centerX;
    g_Camera.ScreenCenterY = slot.centerY;
    g_Camera.PerspectiveX = slot.perspective;
    g_Camera.PerspectiveY = slot.perspective;
    g_fScreenRate_x = g_fScreenRate_y = slot.pixelsPerUnit;
    g_fScreenOffset_x = slot.offsetX;
    g_fScreenOffset_y = slot.offsetY;
    MouseX = pointerInSlot ? static_cast<int>(slot.slotX) + POINTER_INSIDE_OFFSET : POINTER_OUTSIDE;
    MouseY = pointerInSlot ? static_cast<int>(slot.slotY) + POINTER_INSIDE_OFFSET : POINTER_OUTSIDE;

    RenderItem3D(slot.slotX, slot.slotY, slot.slotWidth, slot.slotHeight, subject.itemType, subject.look.level,
                 subject.look.excellentFlags, subject.look.ancientDiscriminator, false);
}

void CItemPreviewScene::DrawGround()
{
    DrawGroundPatch(m_droppedItem.Object.Position);
    if (IsLoaded(m_droppedItem.Object.Type))
        RenderDroppedItem(&m_droppedItem, 0);
}

void CItemPreviewScene::DrawEquipped()
{
    vec3_t spot;
    StudioSpot(spot);
    DrawGroundPatch(spot);
    m_character.AnimateAndRender();
    DrawEffects();
}

void CItemPreviewScene::Release()
{
    m_character.Release();
}
} // namespace Editor::ItemEditor

#endif // _EDITOR
