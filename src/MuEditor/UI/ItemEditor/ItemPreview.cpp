#include "stdafx.h"

#ifdef _EDITOR

#include "ItemPreview.h"

#include "Core/ModelCopy.h"
#include "Core/ScopedOffscreenCapture.h"
#include "Editing/ItemCapturePlan.h" // FaceYawDegrees
#include "Editing/PreviewOutfit.h"
#include "Editing/PreviewSlot.h"

#include "Engine/Object/ZzzCharacter.h"  // WaitCharactersAnimation
#include "Engine/Object/ZzzInfomation.h" // ItemAttribute
#include "Render/Renderer/MuRenderer.h"
#include "UI/NewUI/NewUISystem.h" // g_pOption

#include <algorithm>

namespace
{
using Editor::ItemEditor::CItemPreviewScene;
using Editor::ItemEditor::PreviewView;
using Editor::ItemEditor::SubjectFrame;
namespace Preview = Editor::Preview;

constexpr int MAX_ITEM_LEVEL = 15;
// Any excellent option lights the excellent shine (RenderPartObjectEffect tests
// ExcellentFlags & 63); any set number the ancient one.
constexpr int PREVIEW_EXCELLENT_FLAGS = 1;
constexpr int PREVIEW_ANCIENT_DISCRIMINATOR = 1;
constexpr int HIGHEST_RENDER_LEVEL = 4;
// Frames without the preview on screen before its target and character are freed.
constexpr int FRAMES_BEFORE_RELEASE = 3;

constexpr float BUTTON_PITCH = 10.0f;
constexpr float ZOOM_BUTTON_STEPS = 2.0f; // wheel notches per click of - or +

constexpr ImU32 PICTURE_BORDER = IM_COL32(110, 110, 120, 255);
constexpr ImU32 LABEL_BACKGROUND = IM_COL32(0, 0, 0, 170);
constexpr ImU32 LABEL_TEXT = IM_COL32(240, 240, 240, 255);
constexpr ImU32 PICTURE_WAITING = IM_COL32(24, 24, 28, 255);
constexpr ImU32 SLOT_LINE = IM_COL32(150, 140, 110, 160);
constexpr float SLOT_LINE_THICKNESS = 1.0f;
constexpr ImVec4 NOTE_COLOR{0.75f, 0.75f, 0.75f, 1.0f};

struct ViewChoice
{
    const char* label;
    PreviewView view;
};
constexpr ViewChoice VIEW_CHOICES[] = {
    {"Turntable", PreviewView::Turntable},
    {"Inventory", PreviewView::Inventory},
    {"Ground", PreviewView::Ground},
    {"Equipped", PreviewView::Equipped},
};

bool UsesOrbit(PreviewView view)
{
    return view != PreviewView::Inventory;
}

// Whether a change of subject should put the camera back on the subject's first
// side: a new item, view or character, not a new +level or option.
bool NeedsNewCamera(const Editor::ItemEditor::PreviewSubject& before, const Editor::ItemEditor::PreviewSubject& after)
{
    return before.itemType != after.itemType || before.view != after.view ||
           before.characterClass != after.characterClass;
}
} // namespace

CItemPreview& CItemPreview::GetInstance()
{
    static CItemPreview instance;
    return instance;
}

void CItemPreview::SetCompare(PreviewCompare compare)
{
    m_nextCompare = std::move(compare);
}

void CItemPreview::Render(const Editor::Items::BrowseRow& row, int filterClass, int filterStage)
{
    m_compare = std::move(m_nextCompare);
    m_nextCompare.reset();
    UpdateSubject(row, filterClass, filterStage);
    m_renderedVersion = m_settingsVersion;
    m_showsModel = row.hasModel;
    RenderViewChoice();
    const float width = ImGui::GetContentRegionAvail().x;
    const float side = m_compare ? (width - ImGui::GetStyle().ItemSpacing.x) * 0.5f : width;
    m_wantedSize = m_targetOverride > 0 ? m_targetOverride : Preview::TargetSize(side);
    RenderPictures(side);
    if (UsesOrbit(m_view))
        RenderCameraButtons();
    else
        ImGui::TextColored(NOTE_COLOR, "As the inventory draws it; point at it to turn it.");
    RenderLookControls(row);
    if (m_view == PreviewView::Equipped)
        RenderEquippedControls(filterClass);
    m_requested = row.hasModel;
}

void CItemPreview::UpdateSubject(const Editor::Items::BrowseRow& row, int filterClass, int filterStage)
{
    m_subject.itemType = row.type;
    m_subject.view = m_view;
    m_subject.look.level = m_level;
    m_subject.look.excellentFlags = m_excellent ? PREVIEW_EXCELLENT_FLAGS : 0;
    m_subject.look.ancientDiscriminator = m_ancient ? PREVIEW_ANCIENT_DISCRIMINATOR : 0;
    m_subject.showEveryEffect = m_showEveryEffect;
    m_subject.safeZone = m_safeZone;
    m_classChoice =
        m_classOverride ? *m_classOverride : Preview::PreviewClass(row.requireClass, filterClass, filterStage);
    m_subject.characterClass = Preview::ClassAtStage(m_classChoice.baseClass, m_classChoice.stage);
    m_slotCellsWide = std::max(1, static_cast<int>(ItemAttribute[row.type].Width));
    m_slotCellsHigh = std::max(1, static_cast<int>(ItemAttribute[row.type].Height));
}

void CItemPreview::RenderViewChoice()
{
    for (const ViewChoice& choice : VIEW_CHOICES)
    {
        if (&choice != &VIEW_CHOICES[0])
            ImGui::SameLine();
        if (ImGui::RadioButton(choice.label, m_view == choice.view))
            SetView(choice.view);
    }
}

void CItemPreview::RenderPictures(float side)
{
    PictureInput input;
    RenderPicture(side, m_texture, m_textureSize, "##PreviewPicture", m_compare ? m_compare->leftLabel.c_str() : nullptr,
                  input);
    if (m_compare)
    {
        ImGui::SameLine();
        const bool drawn = DrawsCompare();
        RenderPicture(side, drawn ? m_compareTexture : 0, m_compareTextureSize, "##ComparePicture",
                      m_compare->rightLabel.c_str(), input, drawn ? nullptr : m_compare->rightMessage.c_str());
    }
    ApplyPictureInput(input);
}

void CItemPreview::RenderPicture(float side, std::uint32_t texture, int textureSize, const char* id, const char* label,
                                 PictureInput& input, const char* message)
{
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    const ImVec2 farCorner(corner.x + side, corner.y + side);
    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    void* const picture = texture != 0 ? mu::GetRenderer().GetTexturePointer(texture) : nullptr;
    if (picture != nullptr && textureSize > 0 && m_showsModel)
        drawList->AddImage((ImTextureID)(intptr_t)picture, corner, farCorner);
    else
        drawList->AddRectFilled(corner, farCorner, PICTURE_WAITING);
    if (!m_showsModel)
        drawList->AddText(ImVec2(corner.x + padding.x, corner.y + padding.y), ImGui::GetColorU32(NOTE_COLOR),
                          "The client loads no model for this item.");
    drawList->AddRect(corner, farCorner, PICTURE_BORDER);
    if (m_view == PreviewView::Inventory)
        RenderSlotGrid(corner, side);
    if (message != nullptr)
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                          ImVec2(corner.x + padding.x, corner.y + side * 0.5f), ImGui::GetColorU32(NOTE_COLOR), message,
                          nullptr, side - 2.0f * padding.x);
    if (label != nullptr && m_showsModel)
    {
        const ImVec2 size = ImGui::CalcTextSize(label);
        const ImVec2 textCorner(corner.x + padding.x, corner.y + padding.y);
        drawList->AddRectFilled(ImVec2(corner.x, corner.y),
                                ImVec2(textCorner.x + size.x + padding.x, textCorner.y + size.y + padding.y),
                                LABEL_BACKGROUND);
        drawList->AddText(textCorner, LABEL_TEXT, label);
    }

    ImGui::InvisibleButton(id, ImVec2(side, side));
    // The wheel zooms the picture; without this the window around it scrolls instead.
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    input.hovered = input.hovered || ImGui::IsItemHovered();
    input.dragging = input.dragging || (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f));
}

// The slot's cells over the inventory view, where the game draws its slot frame.
void CItemPreview::RenderSlotGrid(const ImVec2& corner, float side) const
{
    const Preview::InventoryMetrics metrics{1.0f, 1.0f, 1.0f}; // only the layout is used
    const int pixels = std::max(1, static_cast<int>(side));
    const Preview::SlotProjection slot = Preview::SlotInTarget(pixels, pixels, m_slotCellsWide, m_slotCellsHigh, metrics);
    const float cell = slot.pixelsPerUnit;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin(corner.x + slot.offsetX, corner.y + slot.offsetY);
    for (int column = 0; column <= m_slotCellsWide; ++column)
    {
        const float x = origin.x + column * cell;
        drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + m_slotCellsHigh * cell), SLOT_LINE,
                          SLOT_LINE_THICKNESS);
    }
    for (int row = 0; row <= m_slotCellsHigh; ++row)
    {
        const float y = origin.y + row * cell;
        drawList->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + m_slotCellsWide * cell, y), SLOT_LINE,
                          SLOT_LINE_THICKNESS);
    }
}

// One camera for both pictures: dragging or zooming either turns both.
void CItemPreview::ApplyPictureInput(const PictureInput& input)
{
    const ImGuiIO& io = ImGui::GetIO();
    m_pointerInSlot = input.hovered;
    m_dragging = input.dragging;
    if (!UsesOrbit(m_view))
        return;
    if (m_dragging)
        m_orbit = Preview::Dragged(m_orbit, io.MouseDelta.x, io.MouseDelta.y);
    if (input.hovered && io.MouseWheel != 0.0f)
        m_orbit = Preview::Zoomed(m_orbit, io.MouseWheel);
    if (m_autoTurn && !m_dragging)
        m_orbit = Preview::Turned(m_orbit, io.DeltaTime);
}

void CItemPreview::RenderCameraButtons()
{
    // Front is the item's broad face, as in the request captures (Editing/ItemCapturePlan.h).
    const float front = Preview::FaceYawDegrees(m_subject.itemType / Editor::Assets::ITEMS_PER_GROUP);
    const struct
    {
        const char* label;
        float offset;
    } sides[] = {{"Front", 0.0f}, {"Side", Preview::SIDE_OFFSET_DEGREES}, {"Back", Preview::BACK_OFFSET_DEGREES}};
    for (const auto& side : sides)
    {
        if (ImGui::SmallButton(side.label))
        {
            m_orbit.yawDegrees = front + side.offset;
            m_orbit.pitchDegrees = BUTTON_PITCH;
            m_autoTurn = false;
        }
        ImGui::SameLine();
    }
    if (ImGui::SmallButton("Reset"))
        m_hasPrepared = false; // the next draw frames the subject again
    ImGui::SameLine();
    if (ImGui::SmallButton(" - "))
        m_orbit = Preview::Zoomed(m_orbit, -ZOOM_BUTTON_STEPS);
    ImGui::SameLine();
    if (ImGui::SmallButton(" + "))
        m_orbit = Preview::Zoomed(m_orbit, ZOOM_BUTTON_STEPS);
    ImGui::SameLine();
    ImGui::Checkbox("Turn", &m_autoTurn);
    ImGui::TextColored(NOTE_COLOR, "Drag to turn, wheel or - + to zoom.");
}

void CItemPreview::RenderLookControls(const Editor::Items::BrowseRow& row)
{
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderInt("##PreviewLevel", &m_level, 0, MAX_ITEM_LEVEL, "+%d");
    const Editor::Assets::ItemCatalogEntry* catalog = row.catalog;
    ImGui::Checkbox("Excellent", &m_excellent);
    if (catalog != nullptr && !catalog->badges.excellent)
    {
        ImGui::SameLine();
        ImGui::TextColored(NOTE_COLOR, "(never in the game)");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Ancient", &m_ancient);
    if (catalog != nullptr && !catalog->badges.set)
    {
        ImGui::SameLine();
        ImGui::TextColored(NOTE_COLOR, "(no set)");
    }
    const int renderLevel = g_pOption->GetRenderLevel();
    ImGui::Checkbox("Every +level effect", &m_showEveryEffect);
    ImGui::SameLine();
    ImGui::TextColored(NOTE_COLOR, "(game option: %d of %d)", renderLevel, HIGHEST_RENDER_LEVEL);
    if (m_excellent && m_ancient)
        ImGui::TextColored(NOTE_COLOR, "The game shows excellent over ancient.");
}

void CItemPreview::RenderEquippedControls(int filterClass)
{
    const char* source = filterClass == Editor::Items::ANY_CLASS ? "the first class that may use it" : "the class filter";
    ImGui::TextColored(NOTE_COLOR, "%s %d (%s), %s.", Editor::Items::ClassLabel(m_classChoice.baseClass),
                       m_classChoice.stage, source, Preview::WearingLabel(m_scene.Wearing()));
    ImGui::Checkbox("In a town (weapons on the back)", &m_safeZone);
}

void CItemPreview::PrepareIfChanged()
{
    if (m_hasPrepared && m_subject == m_prepared && !m_modelsChanged)
        return;
    const bool newCamera = !m_hasPrepared || NeedsNewCamera(m_prepared, m_subject);
    m_modelsChanged = false;
    m_frame = m_scene.Prepare(m_subject);
    if (newCamera)
        m_orbit = Preview::Facing(m_frame.towardCamera);
    m_prepared = m_subject;
    m_hasPrepared = true;
}

void CItemPreview::RenderPending()
{
    if (!m_requested)
    {
        const bool holdsSomething = m_texture != 0 || m_hasPrepared;
        if (holdsSomething && ++m_framesNotShown >= FRAMES_BEFORE_RELEASE)
            Release();
        return;
    }
    m_requested = false;
    m_framesNotShown = 0;
    if (!DrawsCompare() && m_hasComparePrepared)
    {
        m_compareScene.Release(); // its character may wear a copy that is about to go
        m_hasComparePrepared = false;
    }
    if (!DrawsCompare() && m_compareTexture != 0 && ++m_framesWithoutCompare >= FRAMES_BEFORE_RELEASE)
        ReleaseCompare();
    if (!mu::GetRenderer().IsFrameActive() || m_subject.itemType < 0)
        return;

    PrepareIfChanged();
    if (DrawsCompare())
        PrepareCompareIfChanged();
    KeepPinnedOrbit();
    const SubjectFrame frame = DrawsCompare() ? SharedFrame() : m_frame;
    if (!DrawPicture(m_scene, m_texture, m_textureSize, frame))
        return;
    if (DrawsCompare())
        DrawComparePicture(frame);
    m_drawnVersion = m_renderedVersion;
}

bool CItemPreview::DrawPicture(CItemPreviewScene& scene, std::uint32_t& texture, int& textureSize,
                               const SubjectFrame& frame)
{
    const int size = m_wantedSize;
    const std::uint32_t target = mu::GetRenderer().BeginOffscreenCapture(texture, size, size);
    if (target == 0)
        return false;
    const ScopedOffscreenCapture endCapture;
    texture = target;
    textureSize = size;
    const Preview::View camera = Preview::OrbitView(frame.center, frame.radius, m_orbit, 1.0f);
    scene.Draw(m_subject, camera, size, m_pointerInSlot);
    return true;
}

// The game's characters animate on worker threads that read Models[]; they must be
// done before a copy is swapped in.
void CItemPreview::PrepareCompareIfChanged()
{
    m_framesWithoutCompare = 0;
    if (m_hasComparePrepared && m_comparePrepared == m_subject && m_compareGeneration == m_compare->generation &&
        !m_compareModelsChanged)
        return;
    m_compareModelsChanged = false;
    WaitCharactersAnimation();
    const Editor::Assets::HotReload::ScopedModelSwap swap(m_compare->copies);
    m_compareFrame = m_compareScene.Prepare(m_subject);
    m_comparePrepared = m_subject;
    m_compareGeneration = m_compare->generation;
    m_hasComparePrepared = true;
}

// Both pictures look at the client's item from the same place, far enough for the
// bigger of the two: a candidate that is larger or moved shows as such.
SubjectFrame CItemPreview::SharedFrame() const
{
    SubjectFrame frame = m_frame;
    frame.radius = std::max(m_frame.radius, m_compareFrame.radius);
    return frame;
}

void CItemPreview::DrawComparePicture(const SubjectFrame& frame)
{
    WaitCharactersAnimation();
    const Editor::Assets::HotReload::ScopedModelSwap swap(m_compare->copies);
    DrawPicture(m_compareScene, m_compareTexture, m_compareTextureSize, frame);
}

void CItemPreview::ReleaseCompare()
{
    if (m_compareTexture != 0)
        mu::GetRenderer().ReleaseTexture(m_compareTexture);
    m_compareTexture = 0;
    m_compareTextureSize = 0;
    m_compareScene.Release();
    m_hasComparePrepared = false;
    m_framesWithoutCompare = 0;
}

// A scripted orbit wins over the framing PrepareIfChanged() gives a new subject,
// until the subject it was set for has been drawn.
void CItemPreview::KeepPinnedOrbit()
{
    if (!m_pinnedOrbit)
        return;
    m_orbit = *m_pinnedOrbit;
    if (m_renderedVersion >= m_pinnedVersion)
        m_pinnedOrbit.reset();
}

CItemPreview::Settings CItemPreview::GetSettings() const
{
    Settings settings;
    settings.view = m_view;
    settings.orbit = m_orbit;
    settings.level = m_level;
    settings.excellent = m_excellent;
    settings.ancient = m_ancient;
    settings.showEveryEffect = m_showEveryEffect;
    settings.safeZone = m_safeZone;
    settings.autoTurn = m_autoTurn;
    settings.characterClass = m_classOverride;
    settings.targetSize = m_targetOverride;
    return settings;
}

void CItemPreview::ApplySettings(const Settings& settings)
{
    m_view = settings.view;
    m_level = std::clamp(settings.level, 0, MAX_ITEM_LEVEL);
    m_excellent = settings.excellent;
    m_ancient = settings.ancient;
    m_showEveryEffect = settings.showEveryEffect;
    m_safeZone = settings.safeZone;
    m_classOverride = settings.characterClass;
    m_targetOverride = settings.targetSize;
    SetOrbit(settings.orbit);
    m_autoTurn = settings.autoTurn;
}

void CItemPreview::SetView(PreviewView view)
{
    m_view = view;
    Changed();
}

void CItemPreview::SetOrbit(const Preview::Orbit& orbit)
{
    m_orbit = orbit;
    m_autoTurn = false;
    Changed();
    m_pinnedOrbit = orbit;
    m_pinnedVersion = m_settingsVersion;
}

void CItemPreview::SetLevel(int level)
{
    m_level = std::clamp(level, 0, MAX_ITEM_LEVEL);
    Changed();
}

void CItemPreview::SetExcellent(bool excellent)
{
    m_excellent = excellent;
    Changed();
}

void CItemPreview::SetAncient(bool ancient)
{
    m_ancient = ancient;
    Changed();
}

void CItemPreview::SetCharacterClass(const std::optional<Preview::ClassChoice>& choice)
{
    m_classOverride = choice;
    Changed();
}

void CItemPreview::SetTargetSize(int pixels)
{
    m_targetOverride = pixels > 0 ? Preview::TargetSize(static_cast<float>(pixels)) : 0;
    Changed();
}

void CItemPreview::ModelsChanged()
{
    m_modelsChanged = true;
    m_compareModelsChanged = true;
}

void CItemPreview::Release()
{
    if (m_texture != 0)
        mu::GetRenderer().ReleaseTexture(m_texture);
    m_texture = 0;
    m_textureSize = 0;
    m_scene.Release();
    m_hasPrepared = false;
    ReleaseCompare();
}

#endif // _EDITOR
