#ifndef EYEOVERLAY_H
#define EYEOVERLAY_H

#include <wx/wx.h>
#include <wx/geometry.h>
#include <cstdint>
#include <vector>
#include <memory>
#include "gazetracker.h"
#include "keyboardview.h"
#include "textinputengine.h"
#include "CircularButton.h"
#include "settings.h"
#ifdef USE_ESPEAK
#include "espeakengine.h"
#endif

/**
 * @brief Full-screen transparent overlay providing eye-controlled interface (HeyEyeControl style)
 *
 * Features:
 * - Transparent background with stay-on-top behavior
 * - Gaze cursor visualization with dwell-time progress arc
 * - Circular buttons in radial pattern (mimics HeyEyeControl)
 * - Buttons appear on dwell at center screen
 * - Keyboard toggle button in top-left corner
 */
class EyeOverlay : public wxFrame
{
public:
    explicit EyeOverlay(GazeTracker* gazeTracker, wxWindow *parent);
    ~EyeOverlay();

    // Keyboard visibility
    void ShowKeyboard(bool show);
    bool IsKeyboardVisible() const;

    // Text engine access
    TextInputEngine* GetTextEngine() { return m_textEngine; }

protected:
    void OnPaint(wxPaintEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnClose(wxCloseEvent& event);

private:
    // Event handlers
    void OnGazePositionUpdated(float x, float y, uint64_t timestamp);
    void OnLetterSelected(wxChar letter);
    void OnSwipeCompleted(const std::vector<std::pair<float, float>>& path);
    void OnSpacePressed();
    void OnBackspacePressed();
    void OnDeleteWordPressed();
    void OnEnterPressed();
    void OnSpeakPressed();
    void OnTextChanged(const wxString& text);

    // Button handlers
    void OnSpeak(wxCommandEvent& event);

    void SetupUI();
    void UpdateButtonPositions();

    GazeTracker* m_gazeTracker;
    KeyboardView* m_keyboard;
    TextInputEngine* m_textEngine;
    Settings* m_settings;
#ifdef USE_ESPEAK
    ESpeakEngine* m_espeakEngine;
#endif

    // Circular buttons (HeyEyeControl style)
    std::vector<std::unique_ptr<CircularButton>> m_visibleButtons;

    // Workflow button tracking (UNDO, SUBMIT, SUBMIT_RETURN)
    // Note: Main keyboard keys are managed by m_keyboard (KeyboardView)
    // These are separate workflow buttons for text submission
    struct KeyboardKey {
        wxString label;
        wxRect bounds;
        float dwellProgress;  // 0.0 to 1.0
        KeyboardKey(const wxString& lbl, const wxRect& r) : label(lbl), bounds(r), dwellProgress(0.0f) {}
    };
    std::vector<KeyboardKey> m_keyboardKeys;  // Workflow buttons only

    // Gaze tracking state
    bool m_visible;  // Like HeyEyeControl - when false, window doesn't draw anything
    bool m_keyboardVisible;
    wxPoint2DDouble m_gazePosition;
    uint64_t m_lastGazeTimestamp;
    uint64_t m_previousTimestamp;

    // Screenshot state
    wxBitmap m_screenshot;
    bool m_hasScreenshot;
    wxPoint m_screenshotPosition;  // Current target position (gets refined during zoom)
    wxRect m_screenshotSourceRect;  // Actual area captured in screenshot (for crosshair calculation)
    bool m_isZoomed;                // Whether in zoomed view mode
    float m_settingZoomFactor;      // Zoom magnification (default: 3.0)
    bool m_isScrollMode;            // Whether in scroll mode (from HeyEyeControl)
    bool m_isDragMode;              // Whether in drag mode (mouse button held down)
    bool m_isHiddenMode;            // Whether in hidden mode (minimal UI, UnHide at top)

    // Dwell detection
    std::vector<wxPoint2DDouble> m_positionHistory;
    std::vector<uint64_t> m_timestampHistory;
    float m_dwellProgress;  // 0.0 to 1.0

    // Z-order management (keep overlay on top)
    uint64_t m_lastBringToFrontTimestamp;  // Throttle SetWindowPos calls (only when no buttons visible)

    // Settings (from HeyEyeControl)
    int m_settingWaitTime;              // Default 800ms
    int m_settingHoldTime;              // Default 800ms
    int m_settingCursorDelay;           // Default 50ms
    int m_settingsColorR;               // Default 102
    int m_settingsColorG;               // Default 204
    int m_settingsColorB;               // Default 255
    int m_settingBackgroundOpacity;     // Default 170
    int m_settingSelectionWidth;        // Default 300
    int m_settingSelectionHeight;       // Default 300

    // Helper methods
    void CaptureScreenshotIfNeeded();  // Capture screenshot at current gaze position if not already captured
    void CreateButtonsAtCenter();
    void ClearAllButtons();
    bool UpdateDwellDetection(float x, float y, uint64_t timestamp);  // Returns true if visual state changed
    void DrawButtonWithGC(wxGraphicsContext* gc, CircularButton* button, const wxColour& color);
    void DrawKeyboardWithGC(wxGraphicsContext* gc, const wxColour& color);  // Draw keyboard from KeyboardView + workflow buttons
    void HandleKeyActivation(const wxString& keyLabel);  // Handle workflow button press (UNDO/SUBMIT)
    void EnsureOnTop();  // Bring window to topmost position (throttled)
    bool IsTextCursorAtPosition(int x, int y);  // Check if cursor at position is I-beam (text edit cursor)

    // Mouse interaction methods (from HeyEyeControl)
    void Click();
    void ClickRight();
    void DoubleClick();
    void ToggleScroll();
    void Drag();
    void Drop();
    void ToggleHide();
    void SubmitText();  // Click and send keyboard input (without RETURN)
    void SubmitTextWithReturn();  // Click, send keyboard input, and press RETURN

#ifdef __WXMSW__
    // Windows-specific: Window procedure to prevent focus activation
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    WNDPROC m_oldWndProc;
#endif

    wxDECLARE_EVENT_TABLE();
};

#endif // EYEOVERLAY_H
