#ifndef GAZETRACKER_H
#define GAZETRACKER_H

#include <wx/wx.h>
#include <wx/timer.h>
#include <wx/display.h>
#include <functional>
#include <cstdint>

// Forward declaration for Tobii types
struct tobii_api_t;
struct tobii_device_t;

/**
 * @brief Manages Tobii eye tracking device and gaze position updates
 *
 * Features:
 * - Automatic device discovery and connection
 * - High-frequency gaze updates (~120Hz)
 * - Screen coordinate mapping
 * - Connection state management
 */
class GazeTracker : public wxEvtHandler
{
public:
    explicit GazeTracker();
    ~GazeTracker();

    // Initialization
    bool Initialize();
    bool IsConnected() const { return m_connected; }

    // Tracking control
    void StartTracking();
    void StopTracking();

    // Device info
    wxString GetDeviceUrl() const { return m_deviceUrl; }

    // Manual position update (for testing without Tobii)
    void SetManualPosition(float x, float y);

    // Callback for gaze position updates (replaces Qt signal)
    std::function<void(float x, float y, uint64_t timestamp)> OnGazePositionUpdated;

private:
    void OnTimer(wxTimerEvent& event);
    void UpdateCallbacks();

    bool DiscoverDevice();

    bool m_connected;
    wxString m_deviceUrl;
    wxTimer* m_updateTimer;
    float m_deviceRatio;

    // Tobii API handles
    tobii_api_t* m_api;
    tobii_device_t* m_device;

    // Manual control for testing
    bool m_manualMode;
    float m_manualX;
    float m_manualY;

    wxDECLARE_EVENT_TABLE();
};

#endif // GAZETRACKER_H
