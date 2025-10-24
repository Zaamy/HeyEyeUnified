#include <wx/wx.h>
#include "gazetracker.h"
#include "eyeoverlay.h"

/**
 * @brief Main wxWidgets application class
 */
class HeyEyeApp : public wxApp
{
public:
    virtual bool OnInit() override;
    virtual int OnExit() override;

private:
    GazeTracker* m_gazeTracker;
    EyeOverlay* m_overlay;
};

wxIMPLEMENT_APP(HeyEyeApp);

bool HeyEyeApp::OnInit()
{
    // Create log file in current directory
    wxLog::SetActiveTarget(new wxLogStderr());
    FILE* logFile = fopen("HeyEyeUnified_debug.log", "w");
    if (logFile) {
        wxLog::SetActiveTarget(new wxLogStderr(logFile));
    }

    wxLogMessage("=== HeyEye Unified - Starting ===");
    wxLogMessage("An integrated eye-tracking text input system");
    wxLogMessage("Combining ML swipe prediction and letter-by-letter input");
    wxLogMessage("");

    // Initialize gaze tracker
    m_gazeTracker = new GazeTracker();
    if (!m_gazeTracker->Initialize()) {
        wxLogWarning("Warning: Gaze tracker initialization failed");
        wxLogMessage("Running in manual mode for testing");
    }

    // Create overlay interface
    m_overlay = new EyeOverlay(m_gazeTracker, nullptr);

    // Show fullscreen
    m_overlay->ShowFullScreen(true);
    m_overlay->Show(true);

    // Initialize text engine (if assets are available)
    wxString assetsPath = wxGetCwd() + wxT("/assets");
    wxLogMessage("Looking for assets in: %s", assetsPath);

    if (m_overlay->GetTextEngine()->Initialize(assetsPath)) {
        wxLogMessage("Text engine initialized successfully");
    } else {
        wxLogWarning("Text engine initialization incomplete");
        wxLogMessage("ML features may not be available");
    }

    wxLogMessage("");
    wxLogMessage("=== Keyboard Shortcuts ===");
    wxLogMessage("K - Toggle keyboard visibility");
    wxLogMessage("M - Switch between Letter-by-Letter and Swipe modes");
    wxLogMessage("ESC - Exit application");
    wxLogMessage("");
    wxLogMessage("=== Application Ready ===");

    return true;
}

int HeyEyeApp::OnExit()
{
    // Cleanup is automatic through wxWidgets parent-child deletion
    wxLogMessage("HeyEye Unified exiting...");
    return 0;
}
