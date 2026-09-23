#include "stdafx.h"

#ifdef _EDITOR

#include "ItemAbCaptures.h"

#include "Assets/EditorText.h"
#include "Core/EditorFiles.h"
#include "Editing/ItemCapturePlan.h"
#include "UI/Console/MuEditorConsoleUI.h"

#include "imgui.h"

#include <cctype>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
constexpr const char* OUTPUT_FOLDER = "item-ab";
constexpr const char* SHEET_SUFFIX = "sheet";
constexpr const char* JPEG_EXTENSION = ".jpg";
constexpr const char* NO_SLUG = "picture";

constexpr ImVec4 ERROR_COLOR{1.0f, 0.45f, 0.4f, 1.0f};

// "pilot A" -> "pilot-a", for file names.
std::string Slug(const std::string& label)
{
    std::string slug;
    for (const unsigned char c : label)
    {
        if (std::isalnum(c))
            slug += static_cast<char>(std::tolower(c));
        else if (!slug.empty() && slug.back() != '-')
            slug += '-';
    }
    while (!slug.empty() && slug.back() == '-')
        slug.pop_back();
    return slug.empty() ? NO_SLUG : slug;
}

bool WriteFile(const fs::path& file, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}
} // namespace

void CItemAbCaptures::Start(int itemType, const std::string& itemKey, bool canBeExcellent, const std::string& leftLabel,
                            const std::string& rightLabel, const fs::path& repoRoot)
{
    m_itemKey = itemKey;
    m_leftSlug = Slug(leftLabel);
    m_rightSlug = Slug(rightLabel);
    if (m_rightSlug == m_leftSlug)
        m_rightSlug += "-right";
    m_folder = repoRoot / "out" / OUTPUT_FOLDER / (itemKey + "-" + Editor::Files::BackupStamp());
    m_result.clear();
    m_error.clear();
    const float faceYaw = Editor::Preview::FaceYawDegrees(itemType / Editor::Assets::ITEMS_PER_GROUP);
    m_run.Start(itemType, Editor::Preview::PlanItemCaptures(canBeExcellent, faceYaw), "", CItemCaptureRun::Sides::Pair);
    m_saving = true;
}

void CItemAbCaptures::Step()
{
    if (m_run.IsRunning())
        m_run.Step();
    if (!m_saving || m_run.IsRunning())
        return;
    m_saving = false;
    if (m_run.HasFailed())
        m_error = m_run.Error();
    else if (m_run.IsDone())
        Save();
}

void CItemAbCaptures::Save()
{
    std::error_code ec;
    fs::create_directories(m_folder, ec);
    const auto& captures = m_run.Captures();
    for (std::size_t i = 0; i < captures.size(); ++i)
    {
        const std::string stem = Editor::Text::PathToUtf8(fs::path(captures[i].fileName).stem());
        const bool written = WriteFile(m_folder / (stem + "-" + m_leftSlug + JPEG_EXTENSION), m_run.Jpegs()[i]) &&
                             WriteFile(m_folder / (stem + "-" + m_rightSlug + JPEG_EXTENSION), m_run.RightJpegs()[i]) &&
                             WriteFile(m_folder / (stem + "-" + SHEET_SUFFIX + JPEG_EXTENSION), m_run.SheetJpegs()[i]);
        if (!written)
        {
            m_error = "Could not write the captures into " + Editor::Text::PathToUtf8(m_folder) + ".";
            return;
        }
    }
    m_result = std::to_string(captures.size()) + " A/B pairs and sheets of " + m_itemKey + " (" + m_leftSlug +
               " | " + m_rightSlug + ") in " + Editor::Text::PathToUtf8(m_folder);
    g_MuEditorConsoleUI.LogEditor("[Items] " + m_result);
}

void CItemAbCaptures::Render()
{
    if (m_run.IsRunning())
        ImGui::Text("Capturing %d of %d (%s)...", m_run.ShotNumber() + 1, m_run.ShotCount(), m_run.CurrentShot());
    if (!m_error.empty())
        ImGui::TextColored(ERROR_COLOR, "%s", m_error.c_str());
    if (m_result.empty())
        return;
    ImGui::TextWrapped("%s", m_result.c_str());
    if (ImGui::SmallButton("Open folder"))
    {
        std::string error;
        if (!Editor::Files::OpenWithSystem(m_folder, error))
            m_error = error;
    }
}

#endif // _EDITOR
