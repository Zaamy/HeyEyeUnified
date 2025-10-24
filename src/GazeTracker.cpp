#include "gazetracker.h"

// Tobii includes would go here when linking
// #include "tobii/tobii.h"
// #include "tobii/tobii_streams.h"

// Refresh rate in milliseconds (8ms ~= 120Hz)
#define REFRESH_DELAY 8

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
    if (m_device) {
        // tobii_device_destroy(m_device);
        m_device = nullptr;
    }

    if (m_api) {
        // tobii_api_destroy(m_api);
        m_api = nullptr;
    }

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
            // Use microseconds timestamp
            uint64_t timestamp = wxGetUTCTimeMillis().GetValue() * 1000;
            OnGazePositionUpdated(static_cast<float>(mousePos.x),
                                static_cast<float>(mousePos.y),
                                timestamp);
        }
        return;
    }

    // Process Tobii callbacks
    if (m_device) {
        // tobii_error_t error = tobii_device_process_callbacks(m_device);
        // if (error != TOBII_ERROR_NO_ERROR) {
        //     wxLogWarning("Failed to process callbacks: %s", tobii_error_message(error));
        // }
    }
}

bool GazeTracker::DiscoverDevice()
{
    wxLogMessage("GazeTracker: Discovering Tobii devices...");

    // TODO: Implement Tobii device discovery
    // This would involve:
    // 1. Creating Tobii API context
    // 2. Enumerating available devices
    // 3. Connecting to first available device
    // 4. Subscribing to gaze point stream

    /*
    Example code structure:

    tobii_error_t error = tobii_api_create(&m_api, nullptr, nullptr);
    if (error != TOBII_ERROR_NO_ERROR) {
        return false;
    }

    // Enumerate devices
    std::vector<wxString> urls;
    error = tobii_enumerate_local_device_urls(m_api,
        [](char const* url, void* user_data) {
            std::vector<wxString>* urls = static_cast<std::vector<wxString>*>(user_data);
            urls->push_back(wxString::FromUTF8(url));
        }, &urls);

    if (urls.empty()) {
        return false;
    }

    m_deviceUrl = urls[0];

    // Connect to device
    error = tobii_device_create(m_api, m_deviceUrl.ToUTF8().data(),
                                 TOBII_FIELD_OF_USE_INTERACTIVE, &m_device);
    if (error != TOBII_ERROR_NO_ERROR) {
        return false;
    }

    // Subscribe to gaze stream
    error = tobii_gaze_point_subscribe(m_device,
        [](tobii_gaze_point_t const* gaze_point, void* user_data) {
            GazeTracker* tracker = static_cast<GazeTracker*>(user_data);
            if (gaze_point->validity == TOBII_VALIDITY_VALID) {
                // Get screen dimensions
                wxDisplay display(wxDisplay::GetFromPoint(wxGetMousePosition()));
                wxRect screenRect = display.GetGeometry();

                // Convert normalized coordinates to screen coordinates
                float x = gaze_point->position_xy[0] * screenRect.GetWidth();
                float y = gaze_point->position_xy[1] * screenRect.GetHeight();

                if (tracker->OnGazePositionUpdated) {
                    tracker->OnGazePositionUpdated(x, y, gaze_point->timestamp_us);
                }
            }
        }, this);

    return error == TOBII_ERROR_NO_ERROR;
    */

    return false; // Return false for now (no Tobii SDK linked)
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

    if (m_device) {
        // Unsubscribe from gaze stream
        // tobii_gaze_point_unsubscribe(m_device);
    }

    m_connected = false;
}
