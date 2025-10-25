#include "gazetracker.h"

#ifdef USE_TOBII
#include "tobii/tobii.h"
#include "tobii/tobii_streams.h"
#endif

// Refresh rate in milliseconds (8ms ~= 120Hz)
#define REFRESH_DELAY 8

#ifdef USE_TOBII
// Tobii callback function for gaze point updates
static void gaze_point_callback(tobii_gaze_point_t const* gaze_point, void* user_data)
{
    if (gaze_point->validity == TOBII_VALIDITY_VALID)
    {
        GazeTracker* tracker = static_cast<GazeTracker*>(user_data);

        // Get screen dimensions
        wxDisplay display(wxDisplay::GetFromPoint(wxGetMousePosition()));
        wxRect screenRect = display.GetGeometry();

        // Convert normalized coordinates (0.0-1.0) to screen coordinates
        float x = gaze_point->position_xy[0] * screenRect.GetWidth();
        float y = gaze_point->position_xy[1] * screenRect.GetHeight();

        // Keep timestamp in microseconds (as expected by dwell calculation)
        uint64_t timestamp = gaze_point->timestamp_us;

        // Call the callback if set
        if (tracker->OnGazePositionUpdated)
        {
            tracker->OnGazePositionUpdated(x, y, timestamp);
        }
    }
}

// Tobii callback function for device URL enumeration
static void url_receiver(char const* url, void* user_data)
{
    wxString* deviceUrl = static_cast<wxString*>(user_data);
    *deviceUrl = wxString::FromUTF8(url);
}
#endif

wxBEGIN_EVENT_TABLE(GazeTracker, wxEvtHandler)
    EVT_TIMER(wxID_ANY, GazeTracker::OnTimer)
wxEND_EVENT_TABLE()

GazeTracker::GazeTracker()
    : wxEvtHandler()
    , m_connected(false)
    , m_updateTimer(nullptr)
    , m_deviceRatio(1.0f)
    , m_api(nullptr)
    , m_device(nullptr)
    , m_manualMode(false)
    , m_manualX(0.0f)
    , m_manualY(0.0f)
    , OnGazePositionUpdated(nullptr)
{
    // Create update timer
    m_updateTimer = new wxTimer(this);
}

GazeTracker::~GazeTracker()
{
    StopTracking();

    // Clean up Tobii resources
#ifdef USE_TOBII
    if (m_device) {
        tobii_gaze_point_unsubscribe(m_device);
        tobii_device_destroy(m_device);
        m_device = nullptr;
    }

    if (m_api) {
        tobii_api_destroy(m_api);
        m_api = nullptr;
    }
#endif

    // Timer is deleted automatically by wxWidgets parent-child relationship
    delete m_updateTimer;
}

bool GazeTracker::Initialize()
{
    wxLogMessage("GazeTracker: Initializing...");

    // Try to discover and connect to Tobii device
    if (!DiscoverDevice()) {
        wxLogWarning("GazeTracker: No Tobii device found, running in manual mode");
        m_manualMode = true;

        // Start timer even in manual mode for mouse tracking
        StartTracking();
        return true; // Allow running without device for testing
    }

    // Start tracking
    StartTracking();

    m_connected = true;

    wxLogMessage("GazeTracker: Initialization complete");
    return true;
}

void GazeTracker::SetManualPosition(float x, float y)
{
    m_manualX = x;
    m_manualY = y;

    if (m_manualMode && OnGazePositionUpdated) {
        OnGazePositionUpdated(x, y, 0);
    }
}

void GazeTracker::OnTimer(wxTimerEvent& event)
{
    wxUnusedVar(event);
    UpdateCallbacks();
}

void GazeTracker::UpdateCallbacks()
{
    if (m_manualMode) {
        // In manual mode, use current mouse position
        wxPoint mousePos = wxGetMousePosition();
        if (OnGazePositionUpdated) {
            // Use microseconds timestamp (milliseconds * 1000)
            uint64_t timestamp = wxGetUTCTimeMillis().GetValue() * 1000;
            OnGazePositionUpdated(static_cast<float>(mousePos.x),
                                static_cast<float>(mousePos.y),
                                timestamp);
        }
        return;
    }

#ifdef USE_TOBII
    // Process Tobii callbacks
    if (m_device) {
        // Optionally block this thread until data is available
        tobii_error_t error = tobii_wait_for_callbacks(1, &m_device);
        if (error != TOBII_ERROR_NO_ERROR && error != TOBII_ERROR_TIMED_OUT) {
            wxLogWarning("Tobii wait_for_callbacks failed: %s", tobii_error_message(error));
        }
        else {
            // Process callbacks on this thread if data is available
            error = tobii_device_process_callbacks(m_device);
            if (error != TOBII_ERROR_NO_ERROR) {
                wxLogWarning("Failed to process callbacks: %s", tobii_error_message(error));
            }
        }
    }
#endif
}

bool GazeTracker::DiscoverDevice()
{
    wxLogMessage("GazeTracker: Discovering Tobii devices...");

#ifdef USE_TOBII
    // Get Tobii API version
    tobii_version_t version;
    tobii_error_t error = tobii_get_api_version(&version);
    if (error == TOBII_ERROR_NO_ERROR) {
        wxLogMessage("Tobii API version: %d.%d.%d.%d",
                    version.major, version.minor, version.revision, version.build);
    }

    // Create Tobii API context
    error = tobii_api_create(&m_api, nullptr, nullptr);
    if (error != TOBII_ERROR_NO_ERROR) {
        wxLogWarning("Failed to create Tobii API: %s", tobii_error_message(error));
        return false;
    }

    // Enumerate local devices
    error = tobii_enumerate_local_device_urls(m_api, url_receiver, &m_deviceUrl);
    if (error == TOBII_ERROR_NO_ERROR && !m_deviceUrl.IsEmpty()) {
        wxLogMessage("Tobii device found with URL: %s", m_deviceUrl);
    }
    else {
        wxLogWarning("Tobii enumeration failed: %s", tobii_error_message(error));
        tobii_api_destroy(m_api);
        m_api = nullptr;
        return false;
    }

    // Connect to device
    error = tobii_device_create(m_api, m_deviceUrl.ToUTF8().data(),
                               TOBII_FIELD_OF_USE_INTERACTIVE, &m_device);
    if (error != TOBII_ERROR_NO_ERROR) {
        wxLogWarning("Failed to create Tobii device: %s", tobii_error_message(error));
        tobii_api_destroy(m_api);
        m_api = nullptr;
        return false;
    }

    // Subscribe to gaze point stream
    error = tobii_gaze_point_subscribe(m_device, gaze_point_callback, this);
    if (error != TOBII_ERROR_NO_ERROR) {
        wxLogWarning("Failed to subscribe to gaze data: %s", tobii_error_message(error));
        tobii_device_destroy(m_device);
        tobii_api_destroy(m_api);
        m_device = nullptr;
        m_api = nullptr;
        return false;
    }

    // Check if device is paused
    tobii_state_bool_t paused;
    error = tobii_get_state_bool(m_device, TOBII_STATE_DEVICE_PAUSED, &paused);
    if (error != TOBII_ERROR_NO_ERROR) {
        wxLogWarning("Failed to get device state: %s", tobii_error_message(error));
    }
    else if (paused == TOBII_STATE_BOOL_TRUE) {
        wxLogWarning("Tobii device is paused!");
        return false;
    }
    else {
        wxLogMessage("Tobii device is running!");
    }

    return true;
#else
    wxLogMessage("Tobii SDK not enabled - USE_TOBII not defined");
    return false; // Return false if Tobii SDK is not linked
#endif
}

void GazeTracker::StartTracking()
{
    if (m_updateTimer && !m_updateTimer->IsRunning()) {
        m_updateTimer->Start(REFRESH_DELAY);
        wxLogMessage("GazeTracker: Tracking started");
    }
}

void GazeTracker::StopTracking()
{
    if (m_updateTimer && m_updateTimer->IsRunning()) {
        m_updateTimer->Stop();
        wxLogMessage("GazeTracker: Tracking stopped");
    }

#ifdef USE_TOBII
    if (m_device) {
        // Unsubscribe from gaze stream
        tobii_gaze_point_unsubscribe(m_device);
    }
#endif

    m_connected = false;
}
