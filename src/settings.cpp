#include "settings.h"
#include <wx/stdpaths.h>

Settings::Settings()
    : m_config(nullptr)
    , m_waitTime(800)
    , m_holdTime(800)
    , m_zoomFactor(3.0f)
    , m_backgroundOpacity(170)
    , m_colorR(102)
    , m_colorG(204)
    , m_colorB(255)
    , m_selectionWidth(300)
    , m_selectionHeight(300)
{
    // Get user config directory (AppData\Roaming\HeyEye on Windows)
    wxStandardPaths& paths = wxStandardPaths::Get();
    wxString configDir = paths.GetUserDataDir();

    // Create directory if it doesn't exist
    if (!wxDirExists(configDir)) {
        wxFileName::Mkdir(configDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }

    // Create config file path
    wxString configPath = configDir + wxFileName::GetPathSeparator() + wxT("config.ini");

    // Create config object (uses INI format)
    m_config = new wxFileConfig(
        wxT("HeyEyeUnified"),  // App name
        wxEmptyString,         // Vendor name
        configPath,            // Local file
        wxEmptyString,         // Global file (none)
        wxCONFIG_USE_LOCAL_FILE
    );

    wxLogMessage("Config file: %s", configPath);

    // Load settings
    Load();
}

Settings::~Settings()
{
    if (m_config) {
        delete m_config;
    }
}

void Settings::Load()
{
    if (!m_config) return;

    // Timings
    m_waitTime = m_config->ReadLong(wxT("/timings/wait_time"), 800);
    m_holdTime = m_config->ReadLong(wxT("/timings/hold_time"), 800);

    // Zoom
    m_zoomFactor = (float)m_config->ReadDouble(wxT("/zoom/zoom_factor"), 3.0);

    // Rendering
    m_backgroundOpacity = m_config->ReadLong(wxT("/rendering/background_opacity"), 170);
    m_colorR = m_config->ReadLong(wxT("/rendering/color_r"), 102);
    m_colorG = m_config->ReadLong(wxT("/rendering/color_g"), 204);
    m_colorB = m_config->ReadLong(wxT("/rendering/color_b"), 255);
    m_selectionWidth = m_config->ReadLong(wxT("/rendering/selection_print_size_width"), 300);
    m_selectionHeight = m_config->ReadLong(wxT("/rendering/selection_print_size_height"), 300);

    wxLogMessage("Settings loaded: wait=%dms, hold=%dms, zoom=%.1f, opacity=%d, color=(%d,%d,%d), selection=%dx%d",
                 m_waitTime, m_holdTime, m_zoomFactor, m_backgroundOpacity,
                 m_colorR, m_colorG, m_colorB, m_selectionWidth, m_selectionHeight);
}

void Settings::Save()
{
    if (!m_config) return;

    // Timings
    m_config->Write(wxT("/timings/wait_time"), (long)m_waitTime);
    m_config->Write(wxT("/timings/hold_time"), (long)m_holdTime);

    // Zoom
    m_config->Write(wxT("/zoom/zoom_factor"), (double)m_zoomFactor);

    // Rendering
    m_config->Write(wxT("/rendering/background_opacity"), (long)m_backgroundOpacity);
    m_config->Write(wxT("/rendering/color_r"), (long)m_colorR);
    m_config->Write(wxT("/rendering/color_g"), (long)m_colorG);
    m_config->Write(wxT("/rendering/color_b"), (long)m_colorB);
    m_config->Write(wxT("/rendering/selection_print_size_width"), (long)m_selectionWidth);
    m_config->Write(wxT("/rendering/selection_print_size_height"), (long)m_selectionHeight);

    // Flush to disk
    m_config->Flush();

    wxLogMessage("Settings saved to: %s", GetConfigFilePath());
}

wxString Settings::GetConfigFilePath() const
{
    if (!m_config) return wxEmptyString;

    wxStandardPaths& paths = wxStandardPaths::Get();
    wxString configDir = paths.GetUserDataDir();
    return configDir + wxFileName::GetPathSeparator() + wxT("config.ini");
}
