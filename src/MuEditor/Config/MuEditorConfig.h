#pragma once

#ifdef _EDITOR

#include <string>
#include <map>

// Centralized configuration manager for MU Editor
// Handles all editor settings in a unified MuEditor.ini file
class CMuEditorConfig
{
public:
    static CMuEditorConfig& GetInstance();

    // Load/Save all configuration
    void Load();
    void Save();

    // General settings
    std::string GetLanguage() const;
    void SetLanguage(const std::string& language);

    // Item Editor column visibility settings
    bool GetColumnVisibility(const std::string& columnName, bool defaultValue = false) const;
    void SetColumnVisibility(const std::string& columnName, bool visible);

    // Get all item editor column visibility settings
    const std::map<std::string, bool>& GetAllColumnVisibility() const { return m_columnVisibility; }
    void SetAllColumnVisibility(const std::map<std::string, bool>& visibility) { m_columnVisibility = visibility; }

    // The editor UI scale of the toolbar's - / + buttons; 0 until the owner chose one.
    float GetUIScale() const { return m_uiScale; }
    void SetUIScale(float scale) { m_uiScale = scale; }

    // Whether the item studio (--editor --items) fills the screen.
    bool GetStudioFullscreen() const { return m_studioFullscreen; }
    void SetStudioFullscreen(bool fullscreen) { m_studioFullscreen = fullscreen; }

    // Whether the editor and game consoles at the bottom are shown (the toolbar's Console box).
    bool GetShowConsole() const { return m_showConsole; }
    void SetShowConsole(bool show) { m_showConsole = show; }

    // Skill Editor column visibility settings
    const std::map<std::string, bool>& GetSkillEditorColumnVisibility() const { return m_skillEditorColumnVisibility; }
    void SetSkillEditorColumnVisibility(const std::map<std::string, bool>& visibility) { m_skillEditorColumnVisibility = visibility; }

private:
    CMuEditorConfig();

    // Config file path
    const char* m_configPath = "MuEditor/MuEditor.ini";

    // Settings storage
    std::string m_language;
    float m_uiScale = 0.0f;
    bool m_studioFullscreen = false;
    bool m_showConsole = true;
    std::map<std::string, bool> m_columnVisibility;  // Item Editor columns
    std::map<std::string, bool> m_skillEditorColumnVisibility;  // Skill Editor columns

    // Helper functions for INI parsing
    std::string Trim(const std::string& str);
};

#define g_MuEditorConfig CMuEditorConfig::GetInstance()

#endif // _EDITOR
