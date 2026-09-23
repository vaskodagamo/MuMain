#pragma once

#ifdef _EDITOR

#include "ItemCaptureRun.h"

#include <filesystem>
#include <string>

// "Capture A/B sheet" in the Item Editor's A/B compare: the request capture shots
// (front, side, back, three-quarter, inventory, worn, +level glow) of both
// side-by-side pictures, saved as <nn>-<shot>-<left>.jpg, <nn>-<shot>-<right>.jpg
// and <nn>-<shot>-sheet.jpg (both next to each other) into
// <repo>/out/item-ab/<item key>-<time>/. The request contract lets the owner write
// only owner-decision.json into a request folder, so the pictures stay under out/
// (not in git); attach them to a note or a follow-up request by hand.
class CItemAbCaptures
{
public:
    // Starts on the preview's item (`itemType`, catalog key `itemKey`); the labels
    // name the files.
    void Start(int itemType, const std::string& itemKey, bool canBeExcellent, const std::string& leftLabel,
               const std::string& rightLabel, const std::filesystem::path& repoRoot);
    // Once per frame after the preview was rendered.
    void Step();
    // The progress, the result and "Open folder".
    void Render();

    bool IsRunning() const { return m_run.IsRunning(); }

private:
    void Save();

    CItemCaptureRun m_run;
    bool m_saving = false; // a run was started and its pictures are not saved yet
    std::string m_itemKey;
    std::string m_leftSlug;
    std::string m_rightSlug;
    std::filesystem::path m_folder;
    std::string m_result;
    std::string m_error;
};

#endif // _EDITOR
