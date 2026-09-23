#pragma once

#ifdef _EDITOR

// The render state the Map Editor's overlays on the ground draw with: vertex colours
// only (no texture), alpha blended, depth tested but not written, both faces. It is
// set through the ZzzOpenglUtil wrappers, so their state caches stay true, and the
// caller's state goes back through the same wrappers when the scope ends (the SDL GPU
// renderer has no glPushAttrib/glPopAttrib).
class TerrainOverlayState
{
public:
    TerrainOverlayState();
    ~TerrainOverlayState();
    TerrainOverlayState(const TerrainOverlayState&) = delete;
    TerrainOverlayState& operator=(const TerrainOverlayState&) = delete;

private:
    int m_blendType;
    bool m_texture;
    bool m_alphaTest;
    bool m_depthMask;
    bool m_cullFace;
};

#endif // _EDITOR
