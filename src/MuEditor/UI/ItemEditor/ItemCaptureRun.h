#pragma once

#ifdef _EDITOR

#include "ItemPreview.h"

#include "Assets/ItemRequest.h"
#include "Editing/ItemCapturePlan.h"
#include "Render/Renderer/FramePixelReadback.h" // mu::FramePixels

#include <cstdint>
#include <string>
#include <vector>

// Takes an item request's clean captures (Editing/ItemCapturePlan.h) from the
// Browse preview: for each shot it sets the preview's view, camera, +level and
// excellent, waits until the preview's picture shows them, and reads that picture
// back from the GPU (the offscreen target holds only the 3D view - no editor
// panels, no debug text). One shot takes a few frames, so the editor keeps
// drawing and the dialog shows the progress. The owner's preview settings are put
// back at the end, also after a failure or a cancel.
class CItemCaptureRun
{
public:
    // Starts on the preview's current item, which must be `itemType`.
    void Start(int itemType, std::vector<Editor::Preview::CaptureShot> shots, std::string clientCommit);
    // Advances the run; call once per frame after the Browse preview was rendered.
    void Step();
    void Cancel();

    bool IsRunning() const { return m_phase != Phase::Idle && m_phase != Phase::Done && m_phase != Phase::Failed; }
    bool IsDone() const { return m_phase == Phase::Done; }
    bool HasFailed() const { return m_phase == Phase::Failed; }
    const std::string& Error() const { return m_error; }
    int ShotNumber() const { return static_cast<int>(m_next); } // shots finished
    int ShotCount() const { return static_cast<int>(m_shots.size()); }
    const char* CurrentShot() const;

    // The kept captures (file names numbered in order) and their JPEGs.
    const std::vector<Editor::Assets::ItemCaptureInfo>& Captures() const { return m_captures; }
    const std::vector<std::vector<std::uint8_t>>& Jpegs() const { return m_jpegs; }

private:
    enum class Phase
    {
        Idle,
        Apply,      // set the preview up for the next shot
        WaitDrawn,  // until the preview's picture shows the shot
        WaitPixels, // until the GPU read-back arrives
        Done,
        Failed,
    };

    void ApplyShot();
    void WaitDrawn();
    void WaitPixels();
    void KeepShot(const mu::FramePixels& pixels);
    void Fail(const std::string& error);
    void Finish(Phase phase);

    Phase m_phase = Phase::Idle;
    int m_itemType = -1;
    std::vector<Editor::Preview::CaptureShot> m_shots;
    std::size_t m_next = 0;
    std::string m_clientCommit;
    CItemPreview::Settings m_ownerSettings;
    std::uint64_t m_wantedVersion = 0;
    int m_framesWaited = 0;
    int m_drawnFrames = 0;
    int m_readAttempts = 0;
    std::string m_error;
    std::vector<Editor::Assets::ItemCaptureInfo> m_captures;
    std::vector<std::vector<std::uint8_t>> m_jpegs;
};

#endif // _EDITOR
