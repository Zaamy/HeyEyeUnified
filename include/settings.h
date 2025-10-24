#ifndef SETTINGS_H
#define SETTINGS_H

#include <wx/wx.h>
#include <wx/fileconf.h>

/**
 * @brief Settings manager for HeyEyeUnified
 *
 * Stores configuration in user space (AppData on Windows) using INI format.
 * Settings structure matches HeyEyeControl for compatibility.
 */
class Settings
{
public:
    Settings();
    ~Settings();

    // Load settings from config file
    void Load();

    // Save settings to config file
    void Save();

    // Timings
    int GetWaitTime() const { return m_waitTime; }
    void SetWaitTime(int ms) { m_waitTime = ms; }

    int GetHoldTime() const { return m_holdTime; }
    void SetHoldTime(int ms) { m_holdTime = ms; }

    // Zoom
    float GetZoomFactor() const { return m_zoomFactor; }
    void SetZoomFactor(float factor) { m_zoomFactor = factor; }

    // Rendering
    int GetBackgroundOpacity() const { return m_backgroundOpacity; }
    void SetBackgroundOpacity(int opacity) { m_backgroundOpacity = opacity; }

    int GetColorR() const { return m_colorR; }
    int GetColorG() const { return m_colorG; }
    int GetColorB() const { return m_colorB; }
    void SetColor(int r, int g, int b) { m_colorR = r; m_colorG = g; m_colorB = b; }

    int GetSelectionWidth() const { return m_selectionWidth; }
    void SetSelectionWidth(int width) { m_selectionWidth = width; }

    int GetSelectionHeight() const { return m_selectionHeight; }
    void SetSelectionHeight(int height) { m_selectionHeight = height; }

    // Get config file path
    wxString GetConfigFilePath() const;

private:
    // Config file
    wxFileConfig* m_config;

    // Settings (matching HeyEyeControl)
    // timings/
    int m_waitTime;           // Dwell time before interaction (default: 800ms)
    int m_holdTime;           // Hold time for actions (default: 800ms)

    // zoom/
    float m_zoomFactor;       // Magnification level (default: 3.0)

    // rendering/
    int m_backgroundOpacity;  // Overlay opacity (default: 170)
    int m_colorR;             // UI color R (default: 102)
    int m_colorG;             // UI color G (default: 204)
    int m_colorB;             // UI color B (default: 255)
    int m_selectionWidth;     // Screenshot size width (default: 300)
    int m_selectionHeight;    // Screenshot size height (default: 300)
};

#endif // SETTINGS_H
