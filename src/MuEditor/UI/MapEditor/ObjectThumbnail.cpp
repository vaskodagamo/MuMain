#include "stdafx.h"

#ifdef _EDITOR

#include "ObjectThumbnail.h"

#include "Render/Models/ZzzBMD.h"        // BMD / Models[] / BoneTransform / RENDER_TEXTURE / OBB_t
#include "Render/Renderer/MuRenderer.h"  // mu::GetRenderer()
#include "UI/Console/MuEditorConsoleUI.h"
#include "Core/ModelPose.h"
#include "Core/ScopedOffscreenCapture.h"
#include "Editing/PreviewCamera.h"

#include <algorithm>
#include <cstdio>

// Global bone scale the model Transform/Animation multiply by; the game sets it
// per object (Calc_RenderObject). Must be 1 for an un-scaled preview.
extern float BoneScale;

namespace
{
    // Loads a look-at view into the current matrix.
    void LoadLookAt(const Editor::Thumbnail::Camera& camera)
    {
        Editor::Preview::View view;
        view.eye = camera.eye;
        view.center = camera.center;
        view.up = camera.up;
        float matrix[16];
        Editor::Preview::LookAtColumnMajor(view, matrix);
        mu::GetRenderer().LoadMatrix(matrix);
    }
}

CObjectThumbnail& CObjectThumbnail::GetInstance()
{
    static CObjectThumbnail instance;
    return instance;
}

void CObjectThumbnail::BeginFrame()
{
    m_budget = MAX_RENDERS_PER_FRAME;
}

void CObjectThumbnail::FreeTexture(unsigned int tex)
{
    if (tex != 0)
        mu::GetRenderer().ReleaseTexture(tex);
}

void CObjectThumbnail::Invalidate()
{
    for (auto& kv : m_cache)
        if (kv.second != 0)
            mu::GetRenderer().ReleaseTexture(kv.second);
    m_cache.clear();
    m_failCount.clear();
    m_pending.clear();
}

void CObjectThumbnail::Invalidate(int type)
{
    if (const auto it = m_cache.find(type); it != m_cache.end())
    {
        FreeTexture(it->second);
        m_cache.erase(it);
    }
    m_failCount.erase(type);
    std::erase_if(m_pending, [type](const PendingRender& pending) { return pending.type == type; });
}

unsigned int CObjectThumbnail::Get(int type, Editor::Thumbnail::Framing framing)
{
    auto it = m_cache.find(type);
    if (it != m_cache.end())
        return it->second;
    if (m_budget <= 0)
        return 0;  // try again next frame
    const bool queued = std::any_of(m_pending.begin(), m_pending.end(),
                                    [type](const PendingRender& pending) { return pending.type == type; });
    if (!queued)
    {
        m_pending.push_back({type, framing});
        --m_budget;
    }
    return 0;  // result appears once ProcessPendingRequests() has run
}

bool CObjectThumbnail::RequestSlotPreview(int slot)
{
    if (m_scratchPending)
        return false;  // previous request not delivered yet - caller must wait
    m_scratchSlot = slot;
    m_scratchPending = true;
    m_scratchHasResult = false;
    return true;
}

unsigned int CObjectThumbnail::PollSlotPreview()
{
    if (m_scratchPending || !m_scratchHasResult)
        return 0;
    m_scratchHasResult = false;
    return m_scratchResult;
}

void CObjectThumbnail::ProcessPendingRequests()
{
    // No active frame right now (window minimized/occluded/not focused - e.g.
    // the user alt-tabbed away) - BeginOffscreenCapture() can't succeed for
    // ANY request regardless of the model, so don't even try: leave everything
    // queued for next time instead of spending retry budget on a "failure"
    // that has nothing to do with the model itself and will stop happening
    // the moment the window is active again.
    if (!mu::GetRenderer().IsFrameActive())
        return;

    for (const PendingRender& pending : m_pending)
    {
        const int type = pending.type;
        const unsigned int tex = RenderNow(type, pending.framing);
        if (tex != 0)
        {
            m_cache[type] = tex;
            m_failCount.erase(type);
        }
        else if (++m_failCount[type] >= MAX_TRANSIENT_RETRIES)
        {
            // Given up - cache the failure so Get() stops retrying it.
            m_cache[type] = 0;
            m_failCount.erase(type);
        }
        // else: leave uncached - Get() will queue another attempt later.
    }
    m_pending.clear();

    if (m_scratchPending)
    {
        const unsigned int tex = RenderNow(m_scratchSlot, Editor::Thumbnail::Framing::Object);
        if (tex != 0)
        {
            m_scratchResult = tex;
            m_scratchHasResult = true;
            m_scratchPending = false;
            m_scratchFailCount = 0;
        }
        else if (++m_scratchFailCount >= MAX_TRANSIENT_RETRIES)
        {
            m_scratchResult = 0;
            m_scratchHasResult = true;
            m_scratchPending = false;
            m_scratchFailCount = 0;
        }
        // else: leave m_scratchPending set - retries the same slot next frame.
    }
}

unsigned int CObjectThumbnail::RenderNow(int type, Editor::Thumbnail::Framing framing)
{
    if (type < 0)
        return 0;
    BMD* b = &Models[type];
    if (b->NumMeshs <= 0 || b->Meshs == nullptr)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "[Editor] Thumbnail skip: type %d not loaded (NumMeshs=%d, Meshs=%p)",
                 type, b->NumMeshs, static_cast<void*>(b->Meshs));
        g_MuEditorConsoleUI.LogEditor(msg);
        return 0;
    }

    // Renders into its own dedicated texture via the renderer's offscreen capture -
    // draw calls issued before EndOffscreenCapture() never reach the main frame, so
    // this can't interfere with (or be interfered with by) normal game rendering.
    const std::uint32_t tex = mu::GetRenderer().BeginOffscreenCapture(0u, THUMB_SIZE, THUMB_SIZE);
    if (tex == 0u)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "[Editor] Thumbnail FAILED: type %d BeginOffscreenCapture returned 0", type);
        g_MuEditorConsoleUI.LogEditor(msg);
        return 0;
    }
    const ScopedOffscreenCapture endCaptureOnReturn; // EndOffscreenCapture() on every exit path below

    glMatrixMode(GL_PROJECTION); glPushMatrix();
    glMatrixMode(GL_MODELVIEW);  glPushMatrix();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    // Foliage/tree models rely on alpha-blended or alpha-cutout leaf textures to
    // render solid - with blending off, translucent leaf clusters either vanish or
    // render as sparse fragments instead of a filled canopy.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Unlit, full-bright so the texture reads clearly without a scene light.
    b->BodyScale = 1.0f;
    b->BodyOrigin[0] = b->BodyOrigin[1] = b->BodyOrigin[2] = 0.0f;
    b->BodyHeight = 0.0f;
    b->CurrentAction = 0;
    b->LightEnable = false;
    b->ContrastEnable = false;
    b->BodyLight[0] = b->BodyLight[1] = b->BodyLight[2] = 1.0f;

    BoneScale = 1.0f;
    vec3_t angle = { 0.0f, 0.0f, 0.0f };
    vec3_t head  = { 0.0f, 0.0f, 0.0f };
    b->Animation(BoneTransform, 0.0f, 0.0f, 0, angle, head, false, false);
    vec3_t bbMin = { 0, 0, 0 }, bbMax = { 0, 0, 0 };
    OBB_t obb;
    b->Transform(BoneTransform, bbMin, bbMax, &obb, true);
    // Transform() leaves bbMin/bbMax as they are, so world objects are framed as
    // the fallback box (a typical object at the origin), as they always were.
    // Items are framed from their real bounds.
    if (framing == Editor::Thumbnail::Framing::Item && !Editor::ModelPose::Bounds(*b, bbMin, bbMax))
        return 0;

    // Frame the model from its bounding box (see Editing/ThumbnailFraming.h).
    const Editor::Thumbnail::Camera camera = Editor::Thumbnail::FrameBounds(
        {bbMin[0], bbMin[1], bbMin[2]}, {bbMax[0], bbMax[1], bbMax[2]}, framing);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(camera.fovDegrees, 1.0f, camera.zNear, camera.zFar);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    LoadLookAt(camera);

    b->RenderBody(RENDER_TEXTURE, 1.0f, -1, 1.0f, 0.0f, 0.0f);

    // Restore.
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();

    return tex;
}

#endif // _EDITOR
