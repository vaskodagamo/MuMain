#include "stdafx.h"

#ifdef _EDITOR

#include "ItemEditorActions.h"
#include "ItemEditorFiles.h"
#include "Data/DataHandler/ItemData/ItemDataHandler.h"
#include "Data/GameData/ItemData/ItemFieldMetadata.h"
#include "../MuEditor/UI/Console/MuEditorConsoleUI.h"
#include "I18N/All.h"
#include "imgui.h"
#include <string>
#include <sstream>
#include <filesystem>

// ===== HELPER FUNCTIONS =====

void CItemEditorActions::ConvertItemName(char* outBuffer, size_t bufferSize, const wchar_t* name)
{
    WideCharToMultiByte(CP_UTF8, 0, name, -1, outBuffer, (int)bufferSize, NULL, NULL);
}

std::string CItemEditorActions::GetFieldValueAsString(const ITEM_ATTRIBUTE& item, const ItemFieldDescriptor& desc)
{
    std::stringstream ss;

    // Get pointer to the field using offset
    const BYTE* itemPtr = reinterpret_cast<const BYTE*>(&item);
    const void* fieldPtr = itemPtr + desc.offset;

    switch (desc.type)
    {
    case EItemFieldType::Bool:
        ss << (*reinterpret_cast<const bool*>(fieldPtr) ? 1 : 0);
        break;

    case EItemFieldType::Byte:
        ss << (int)*reinterpret_cast<const BYTE*>(fieldPtr);
        break;

    case EItemFieldType::Word:
        ss << *reinterpret_cast<const WORD*>(fieldPtr);
        break;

    case EItemFieldType::Int:
        ss << *reinterpret_cast<const int*>(fieldPtr);
        break;

    case EItemFieldType::WCharArray:
    {
        char buffer[256];
        ConvertItemName(buffer, sizeof(buffer), reinterpret_cast<const wchar_t*>(fieldPtr));
        ss << buffer;
        break;
    }
    }

    return ss.str();
}

// ===== EXPORT FUNCTIONS =====

std::string CItemEditorActions::GetCSVHeader()
{
    std::stringstream ss;
    const ItemFieldDescriptor* fields = GetFieldDescriptors(); const int fieldCount = GetFieldCount();

    ss << "Index";
    for (int i = 0; i < fieldCount; ++i)
    {
        ss << "," << GetFieldDisplayName(fields[i]);
    }

    return ss.str();
}

std::string CItemEditorActions::ExportItemToReadable(int itemIndex, ITEM_ATTRIBUTE& item)
{
    std::stringstream ss;
    const ItemFieldDescriptor* fields = GetFieldDescriptors(); const int fieldCount = GetFieldCount();

    ss << "Row " << itemIndex << "\n";
    ss << "Index = " << itemIndex;

    for (int i = 0; i < fieldCount; ++i)
    {
        ss << ", " << GetFieldDisplayName(fields[i]) << " = ";
        ss << GetFieldValueAsString(item, fields[i]);
    }

    return ss.str();
}

std::string CItemEditorActions::ExportItemToCSV(int itemIndex, ITEM_ATTRIBUTE& item)
{
    std::stringstream ss;
    const ItemFieldDescriptor* fields = GetFieldDescriptors(); const int fieldCount = GetFieldCount();

    ss << itemIndex;

    for (int i = 0; i < fieldCount; ++i)
    {
        ss << ",";

        // Quote strings for CSV
        if (fields[i].type == EItemFieldType::WCharArray)
        {
            ss << "\"" << GetFieldValueAsString(item, fields[i]) << "\"";
        }
        else
        {
            ss << GetFieldValueAsString(item, fields[i]);
        }
    }

    return ss.str();
}

std::string CItemEditorActions::ExportItemCombined(int itemIndex, ITEM_ATTRIBUTE& item)
{
    return ExportItemToReadable(itemIndex, item) + "\n" + ExportItemToCSV(itemIndex, item);
}

// ===== SAVE AND EXPORT =====

std::string CItemEditorActions::s_status;

namespace
{
std::string AbsoluteText(const std::filesystem::path& file)
{
    return Editor::Files::PathToUtf8(Editor::Files::AbsolutePath(file));
}

// Status line of an export: where it went, and its copy in <repo>/out/editor-exports.
std::string DescribeExport(const std::filesystem::path& file)
{
    std::string text = "Exported " + Editor::Files::PathToUtf8(file.filename()) +
                       "\n  game:   " + AbsoluteText(file);
    const std::filesystem::path copy = Editor::Files::CopyToRepoExports(file);
    if (!copy.empty())
        text += "\n  copy:   " + Editor::Files::PathToUtf8(copy);
    return text;
}
} // namespace

void CItemEditorActions::SaveItemTable()
{
    const std::filesystem::path file = Editor::Files::ItemTableFile();
    std::wstring fileName = file.wstring();
    std::string changeLog;
    if (g_ItemDataHandler.Save(fileName.data(), &changeLog))
    {
        // Log change details first, then save completion message
        g_MuEditorConsoleUI.LogEditor(changeLog);
        g_MuEditorConsoleUI.LogEditor("=== SAVE COMPLETED ===");
        s_status = Editor::Files::DescribeSavedFiles({Editor::Files::MirrorSavedFile(file)});
        ImGui::OpenPopup("Save Success");
        return;
    }

    if (changeLog.find("No changes") != std::string::npos)
    {
        g_MuEditorConsoleUI.LogEditor(changeLog);
        s_status = "No changes to save in " + Editor::Files::PathToUtf8(file.filename());
        return;
    }

    g_MuEditorConsoleUI.LogEditor(I18N::Editor::FailedToSaveItems);
    s_status = "Could not write " + AbsoluteText(file);
    ImGui::OpenPopup("Save Failed");
}

void CItemEditorActions::ExportLegacyTable()
{
    const std::filesystem::path file = Editor::Files::ItemLegacyExportFile();
    std::wstring fileName = file.wstring();
    if (!g_ItemDataHandler.ExportAsS6E3(fileName.data()))
    {
        g_MuEditorConsoleUI.LogEditor(I18N::Editor::FailedToExportAsS6E3Format);
        s_status = "Could not write " + AbsoluteText(file);
        ImGui::OpenPopup("Export S6E3 Failed");
        return;
    }

    g_MuEditorConsoleUI.LogEditor("Exported items as S6E3 legacy format: " + AbsoluteText(file));
    s_status = DescribeExport(file);
    ImGui::OpenPopup("Export S6E3 Success");
}

void CItemEditorActions::ExportCsv()
{
    const std::filesystem::path file = Editor::Files::ItemCsvExportFile();
    std::wstring fileName = file.wstring();
    if (!g_ItemDataHandler.ExportToCsv(fileName.data()))
    {
        g_MuEditorConsoleUI.LogEditor(I18N::Editor::FailedToExportAsCSV);
        s_status = "Could not write " + AbsoluteText(file);
        ImGui::OpenPopup("Export CSV Failed");
        return;
    }

    g_MuEditorConsoleUI.LogEditor("Exported items as CSV: " + AbsoluteText(file));
    s_status = DescribeExport(file);
    ImGui::OpenPopup("Export CSV Success");
}

// ===== BUTTON RENDERING =====

void CItemEditorActions::RenderSaveButton()
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));

    if (ImGui::Button(I18N::Editor::SaveItems))
    {
        SaveItemTable();
    }

    ImGui::PopStyleColor(2);
}

void CItemEditorActions::RenderExportS6E3Button()
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.9f, 1.0f));

    if (ImGui::Button(I18N::Editor::ExportAsS6E3))
    {
        ExportLegacyTable();
    }

    ImGui::PopStyleColor(2);
}

void CItemEditorActions::RenderExportCSVButton()
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.7f, 1.0f));

    if (ImGui::Button(I18N::Editor::ExportAsCSV))
    {
        ExportCsv();
    }

    ImGui::PopStyleColor(2);
}

void CItemEditorActions::RenderStatus()
{
    if (s_status.empty())
        return;
    ImGui::TextWrapped("%s", s_status.c_str());
}

void CItemEditorActions::RenderAllButtons()
{
    RenderSaveButton();
    ImGui::SameLine();
    RenderExportS6E3Button();
    ImGui::SameLine();
    RenderExportCSVButton();
    RenderStatus();
}

#endif // _EDITOR
