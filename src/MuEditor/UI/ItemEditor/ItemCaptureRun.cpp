#include "stdafx.h"

#ifdef _EDITOR

#include "ItemCaptureRun.h"

#include "Assets/CaptureImage.h"
#include "Render/Renderer/MuRenderer.h"

namespace
{
using Editor::ItemEditor::PreviewView;
using Editor::Preview::CaptureView;

// The captures' side in pixels: detailed enough for a texture artist, small
// enough for a repository without LFS (the README allows up to 1920).
constexpr int CAPTURE_SIZE = 1024;
// Frames the picture is drawn with the shot's settings before it is read, so a
// character's pose and the glow have settled.
constexpr int SETTLE_FRAMES = 3;
// Give up when the preview has not drawn the shot after this many frames (the
// Browse tab or the Item Editor was closed, or the item changed).
constexpr int MAX_WAIT_FRAMES = 240;
constexpr int MAX_READ_ATTEMPTS = 3;

PreviewView ViewOf(CaptureView view)
{
    switch (view)
    {
    case CaptureView::Inventory:
        return PreviewView::Inventory;
    case CaptureView::Equipped:
        return PreviewView::Equipped;
    case CaptureView::Turntable:
        break;
    }
    return PreviewView::Turntable;
}

bool IsWornOnBody(Editor::Preview::Wearing wearing)
{
    return wearing != Editor::Preview::Wearing::NotShown && wearing != Editor::Preview::Wearing::NotWearable;
}
} // namespace

void CItemCaptureRun::Start(int itemType, std::vector<Editor::Preview::CaptureShot> shots, std::string clientCommit)
{
    if (IsRunning())
        Cancel();
    m_itemType = itemType;
    m_shots = std::move(shots);
    m_next = 0;
    m_clientCommit = std::move(clientCommit);
    m_captures.clear();
    m_jpegs.clear();
    m_error.clear();
    m_ownerSettings = g_ItemPreview.GetSettings();
    m_phase = m_shots.empty() ? Phase::Done : Phase::Apply;
}

void CItemCaptureRun::Step()
{
    switch (m_phase)
    {
    case Phase::Apply:
        ApplyShot();
        break;
    case Phase::WaitDrawn:
        WaitDrawn();
        break;
    case Phase::WaitPixels:
        WaitPixels();
        break;
    case Phase::Idle:
    case Phase::Done:
    case Phase::Failed:
        break;
    }
}

void CItemCaptureRun::Cancel()
{
    if (IsRunning())
        Finish(Phase::Idle);
}

const char* CItemCaptureRun::CurrentShot() const
{
    return m_next < m_shots.size() ? m_shots[m_next].slug.c_str() : "";
}

void CItemCaptureRun::ApplyShot()
{
    const Editor::Preview::CaptureShot& shot = m_shots[m_next];
    CItemPreview::Settings settings = m_ownerSettings;
    settings.view = ViewOf(shot.view);
    settings.orbit = Editor::Preview::Orbit{shot.yawDegrees, shot.pitchDegrees, 1.0f};
    settings.level = shot.level;
    settings.excellent = shot.excellent;
    settings.ancient = false;
    settings.autoTurn = false;
    settings.targetSize = CAPTURE_SIZE;
    g_ItemPreview.ApplySettings(settings);
    m_wantedVersion = g_ItemPreview.SettingsVersion();
    m_framesWaited = 0;
    m_drawnFrames = 0;
    m_readAttempts = 0;
    m_phase = Phase::WaitDrawn;
}

void CItemCaptureRun::WaitDrawn()
{
    if (++m_framesWaited > MAX_WAIT_FRAMES)
    {
        Fail("The preview did not draw the item. Keep the Browse tab open with the item selected, then try again.");
        return;
    }
    const bool shown = g_ItemPreview.ShownItem() == m_itemType && g_ItemPreview.Texture() != 0 &&
                       g_ItemPreview.TextureSize() == CAPTURE_SIZE;
    if (!shown || g_ItemPreview.DrawnSettings() < m_wantedVersion || ++m_drawnFrames < SETTLE_FRAMES)
        return;
    mu::IMuRenderer& renderer = mu::GetRenderer();
    if (renderer.IsTexturePixelsPending())
        return; // a read-back of a cancelled run is still on its way
    mu::FramePixels stale;
    (void)renderer.ConsumeTexturePixels(stale);
    // This frame's picture (drawn before the editor panels) is read at the end of the frame.
    if (renderer.RequestTexturePixels(g_ItemPreview.Texture()))
        m_phase = Phase::WaitPixels;
}

void CItemCaptureRun::WaitPixels()
{
    mu::FramePixels pixels;
    if (mu::GetRenderer().ConsumeTexturePixels(pixels))
    {
        KeepShot(pixels);
        return;
    }
    if (mu::GetRenderer().IsTexturePixelsPending())
        return;
    if (++m_readAttempts >= MAX_READ_ATTEMPTS)
    {
        Fail("The preview picture could not be read back from the GPU.");
        return;
    }
    m_drawnFrames = 0;
    m_phase = Phase::WaitDrawn;
}

void CItemCaptureRun::KeepShot(const mu::FramePixels& pixels)
{
    const Editor::Preview::CaptureShot& shot = m_shots[m_next++];
    const bool keep = !shot.onlyWhenWorn || IsWornOnBody(g_ItemPreview.Wearing());
    if (keep)
    {
        const mu::FramePixels small = Editor::Capture::DownscaleToWidth(pixels, CAPTURE_SIZE);
        std::vector<std::uint8_t> jpeg = Editor::Capture::EncodeJpeg(small, Editor::Capture::CAPTURE_JPEG_QUALITY);
        if (jpeg.empty())
        {
            Fail("A capture could not be encoded as a JPEG.");
            return;
        }
        Editor::Assets::ItemCaptureInfo info;
        info.fileName = Editor::Preview::CaptureFileName(static_cast<int>(m_captures.size()) + 1, shot.slug);
        info.view = shot.requestView;
        info.angle = shot.angle;
        info.itemLevel = shot.level;
        info.excellent = shot.excellent;
        info.width = static_cast<int>(small.width);
        info.height = static_cast<int>(small.height);
        info.clientCommit = m_clientCommit;
        m_captures.push_back(info);
        m_jpegs.push_back(std::move(jpeg));
    }
    if (m_next >= m_shots.size())
        Finish(Phase::Done);
    else
        m_phase = Phase::Apply;
}

void CItemCaptureRun::Fail(const std::string& error)
{
    m_error = error;
    Finish(Phase::Failed);
}

void CItemCaptureRun::Finish(Phase phase)
{
    g_ItemPreview.ApplySettings(m_ownerSettings);
    m_phase = phase;
}

#endif // _EDITOR
