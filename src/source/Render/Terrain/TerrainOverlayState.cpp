#include "stdafx.h"

#ifdef _EDITOR

#include "TerrainOverlayState.h"

#include "Render/Textures/ZzzOpenglUtil.h"

// The wrappers' state caches (ZzzOpenglUtil.cpp) that its header does not export.
extern int AlphaBlendType;
extern bool AlphaTestEnable;

namespace
{
// AlphaBlendType values, each set by the wrapper that switches to that blend.
enum BlendType
{
    BLEND_NONE = 0,       // DisableAlphaBlend
    BLEND_LIGHT_MAP = 1,  // EnableLightMap
    BLEND_ALPHA_TEST = 2, // EnableAlphaTest
    BLEND_GLOW = 3,       // EnableAlphaBlend
    BLEND_MINUS = 4,      // EnableAlphaBlendMinus
    BLEND_LUMINANCE = 5,  // EnableAlphaBlend2
    BLEND_ALPHA = 6,      // EnableAlphaBlend3
    BLEND_MIXED = 7,      // EnableAlphaBlend4
};

// Each blend wrapper also switches the texture on and sets alpha test, face culling
// and depth writes its own way; the caller's own values are put back after it.
void RestoreBlend(int blendType, bool depthMask)
{
    switch (blendType)
    {
    case BLEND_LIGHT_MAP:
        EnableLightMap();
        break;
    case BLEND_ALPHA_TEST:
        EnableAlphaTest(depthMask);
        break;
    case BLEND_GLOW:
        EnableAlphaBlend();
        break;
    case BLEND_MINUS:
        EnableAlphaBlendMinus();
        break;
    case BLEND_LUMINANCE:
        EnableAlphaBlend2();
        break;
    case BLEND_ALPHA:
        EnableAlphaBlend3();
        break;
    case BLEND_MIXED:
        EnableAlphaBlend4();
        break;
    default:
        DisableAlphaBlend();
        break;
    }
}
} // namespace

TerrainOverlayState::TerrainOverlayState()
    : m_blendType(AlphaBlendType), m_texture(TextureEnable), m_alphaTest(AlphaTestEnable), m_depthMask(DepthMaskEnable),
      m_cullFace(CullFaceEnable)
{
    EnableAlphaBlend3(); // source alpha over what is drawn, both faces
    DisableTexture();    // vertex colours only
    DisableDepthMask();  // tint the ground without hiding what is drawn after it
}

TerrainOverlayState::~TerrainOverlayState()
{
    RestoreBlend(m_blendType, m_depthMask);
    if (!m_texture)
        DisableTexture(m_alphaTest);
    if (m_depthMask)
        EnableDepthMask();
    else
        DisableDepthMask();
    if (m_cullFace)
        EnableCullFace();
    else
        DisableCullFace();
}

#endif // _EDITOR
