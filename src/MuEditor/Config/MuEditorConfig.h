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

    // The address of the OpenMU admin panel which the Item Sync window updates from
    // ([OpenMU] AdminPanelUrl).
    const std::string& GetOpenMUAdminPanelUrl() const;
    void SetOpenMUAdminPanelUrl(const std::string& url);

    // The Server List window's preview of the offline servers ([ServerList] ShowOfflineServers,
    // OfflineServerLoad): CServerListManager::SetOfflinePreview.
    bool GetShowOfflineServers() const
    {
        return m_showOfflineServers;
    }
    int GetOfflineServerLoad() const
    {
        return m_offlineServerLoad;
    }
    void SetOfflineServers(bool show, int load)
    {
        m_showOfflineServers = show;
        m_offlineServerLoad = load;
    }

    // The editor UI scale of the menu bar's - / + buttons; 0 until the owner chose one.
    float GetUIScale() const { return m_uiScale; }
    void SetUIScale(float scale) { m_uiScale = scale; }

    // Whether the item studio (--editor --items) fills the screen.
    bool GetStudioFullscreen() const { return m_studioFullscreen; }
    void SetStudioFullscreen(bool fullscreen) { m_studioFullscreen = fullscreen; }

    // Whether the console is shown (Windows > Console).
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
    std::map<std::string, bool> m_skillEditorColumnVisibility;  // Skill Editor columns
    std::string m_openMUAdminPanelUrl;
    bool m_showOfflineServers = false;
    int m_offlineServerLoad = 50;

    // Helper functions for INI parsing
    std::string Trim(const std::string& str);
};

#define g_MuEditorConfig CMuEditorConfig::GetInstance()

#endif // _EDITOR
