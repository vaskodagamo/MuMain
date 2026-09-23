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
// back at the end, also after a failure or a cancel. With the preview side by side
// (A/B compare) a run can take both pictures of every shot and a sheet of the two.
class CItemCaptureRun
{
public:
    enum class Sides
    {
        One,  // the preview's picture
        Pair, // the side-by-side view's left and right picture of each shot
    };

    // Starts on the preview's current item, which must be `itemType`.
    void Start(int itemType, std::vector<Editor::Preview::CaptureShot> shots, std::string clientCommit,
               Sides sides = Sides::One);
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
    // Sides::Pair: the right pictures and the sheets (left | right), in the order of Jpegs().
    const std::vector<std::vector<std::uint8_t>>& RightJpegs() const { return m_rightJpegs; }
    const std::vector<std::vector<std::uint8_t>>& SheetJpegs() const { return m_sheetJpegs; }

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
    bool IsShotDrawn() const;
    bool RequestSide(int side);
    bool KeepRightAndSheet(const mu::FramePixels& left, const mu::FramePixels& right);
    void KeepShot(const mu::FramePixels& left, const mu::FramePixels* right);
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
    Sides m_sides = Sides::One;
    int m_side = 0;                 // the picture being read: 0 left, 1 right
    mu::FramePixels m_leftPixels;   // Sides::Pair: the left picture of the shot
    std::vector<std::vector<std::uint8_t>> m_rightJpegs;
    std::vector<std::vector<std::uint8_t>> m_sheetJpegs;
};

#endif // _EDITOR
