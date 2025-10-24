#include "eyeoverlay.h"
#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/dcscreen.h>
#include <wx/rawbmp.h>
#include <wx/graphics.h>
#include <cmath>  // For std::sqrt

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Button IDs
enum {
    ID_TOGGLE_MODE = wxID_HIGHEST + 1,
    ID_TOGGLE_KEYBOARD,
    ID_DELETE_WORD,
    ID_SPEAK
};

wxBEGIN_EVENT_TABLE(EyeOverlay, wxFrame)
    EVT_PAINT(EyeOverlay::OnPaint)
    EVT_ERASE_BACKGROUND(EyeOverlay::OnEraseBackground)
    EVT_SIZE(EyeOverlay::OnSize)
    EVT_KEY_DOWN(EyeOverlay::OnKeyDown)
    EVT_CLOSE(EyeOverlay::OnClose)
    EVT_BUTTON(ID_TOGGLE_MODE, EyeOverlay::OnToggleInputMode)
    EVT_BUTTON(ID_TOGGLE_KEYBOARD, EyeOverlay::OnToggleKeyboard)
    EVT_BUTTON(ID_DELETE_WORD, EyeOverlay::OnDeleteLastWord)
    EVT_BUTTON(ID_SPEAK, EyeOverlay::OnSpeak)
wxEND_EVENT_TABLE()

EyeOverlay::EyeOverlay(GazeTracker* gazeTracker, wxWindow *parent)
    : wxFrame(parent, wxID_ANY, wxT("HeyEye Unified"),
              wxDefaultPosition, wxDefaultSize,
              wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE | wxFRAME_SHAPED)
    , m_gazeTracker(gazeTracker)
    , m_keyboard(nullptr)
    , m_textEngine(nullptr)
    , m_visible(true)
    , m_keyboardVisible(false)
    , m_gazePosition(0, 0)
    , m_lastGazeTimestamp(0)
    , m_previousTimestamp(0)
    , m_hasScreenshot(false)
    , m_screenshotPosition(0, 0)
    , m_screenshotSourceRect(0, 0, 0, 0)
    , m_isZoomed(false)
    , m_settingZoomFactor(3.0f)
    , m_isScrollMode(false)
    , m_isDragMode(false)
    , m_isHiddenMode(false)
    , m_dwellProgress(0.0f)
    , m_dwellPosition(0, 0)
    , m_lastBringToFrontTimestamp(0)
    , m_settingWaitTime(800)
    , m_settingHoldTime(800)
    , m_settingsColorR(102)
    , m_settingsColorG(204)
    , m_settingsColorB(255)
    , m_settingBackgroundOpacity(170)
    , m_settingSelectionWidth(300)
    , m_settingSelectionHeight(300)
#ifdef __WXMSW__
    , m_oldWndProc(nullptr)
#endif
{
    // Transparent background setup
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxBLACK);

#ifdef __WXMSW__
    // Enable layered window for per-pixel alpha transparency
    // Also add WS_EX_NOACTIVATE to prevent window from stealing focus (like HeyEyeControl Qt::WA_ShowWithoutActivating)
    // Also add WS_EX_TRANSPARENT to let click pass throw the window
    HWND hwnd = (HWND)GetHWND();
    LONG exStyle = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    ::SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);

    // Subclass the window to handle WM_MOUSEACTIVATE and prevent activation
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    m_oldWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);

    // Note: We'll use UpdateLayeredWindow in OnPaint instead of SetLayeredWindowAttributes
#endif

    SetupUI();

    // Connect gaze tracker callback
    m_gazeTracker->OnGazePositionUpdated = [this](float x, float y, uint64_t timestamp) {
        OnGazePositionUpdated(x, y, timestamp);
    };

    // Initialize text engine
    m_textEngine = new TextInputEngine();
    m_textEngine->OnTextChanged = [this](const wxString& text) {
        OnTextChanged(text);
    };

    // Fullscreen on current display
    wxDisplay display(wxDisplay::GetFromPoint(wxGetMousePosition()));
    wxRect screenRect = display.GetGeometry();
    SetSize(screenRect);
    SetPosition(screenRect.GetPosition());

    wxLogMessage("EyeOverlay initialized: %dx%d", screenRect.GetWidth(), screenRect.GetHeight());
}

EyeOverlay::~EyeOverlay() = default;

#ifdef __WXMSW__
// Windows message handler to prevent window from taking focus
LRESULT CALLBACK EyeOverlay::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    EyeOverlay* self = (EyeOverlay*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // Prevent window from being activated when clicked
    if (msg == WM_MOUSEACTIVATE) {
        return MA_NOACTIVATE;
    }

    // Call original window procedure
    if (self && self->m_oldWndProc) {
        return CallWindowProc(self->m_oldWndProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

void EyeOverlay::ShowKeyboard(bool show) {
    m_keyboardVisible = show;
    if (m_keyboard) {
        m_keyboard->Show(show);
    }

    Refresh();
}

bool EyeOverlay::IsKeyboardVisible() const {
    return m_keyboardVisible;
}

void EyeOverlay::OnPaint(wxPaintEvent& event)
{
    wxUnusedVar(event);
    wxSize clientSize = GetClientSize();

    // Create 32-bit bitmap with alpha channel
    wxBitmap bmp(clientSize.GetWidth(), clientSize.GetHeight(), 32);
    bmp.UseAlpha();

    wxMemoryDC mdc(bmp);

    // Use wxGraphicsContext (GDI+) for all drawing - supports proper alpha
    wxGraphicsContext* gc = wxGraphicsContext::Create(mdc);
    if (!gc) return;

    // Clear to fully transparent
    gc->SetCompositionMode(wxCOMPOSITION_CLEAR);
    gc->DrawRectangle(0, 0, clientSize.GetWidth(), clientSize.GetHeight());
    gc->SetCompositionMode(wxCOMPOSITION_OVER);

    // Like HeyEyeControl: if not visible, just draw nothing (fully transparent)
    if (!m_visible) {

        delete gc;
        mdc.SelectObject(wxNullBitmap);

#ifdef __WXMSW__
        // Update with fully transparent bitmap
        HWND hwnd = (HWND)GetHWND();
        HDC hdcScreen = ::GetDC(NULL);
        HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = (HBITMAP)bmp.GetHBITMAP();
        HGDIOBJ oldBitmap = ::SelectObject(hdcMem, hBitmap);

        SIZE size = { clientSize.GetWidth(), clientSize.GetHeight() };
        POINT src = { 0, 0 };
        POINT pos = { GetPosition().x, GetPosition().y };

        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        ::UpdateLayeredWindow(hwnd, hdcScreen, &pos, &size, hdcMem, &src, 0, &blend, ULW_ALPHA);

        ::SelectObject(hdcMem, oldBitmap);
        ::DeleteDC(hdcMem);
        ::ReleaseDC(NULL, hdcScreen);
#endif
        return;
    }

    wxColour buttonColor(m_settingsColorR, m_settingsColorG, m_settingsColorB);

    // Draw semi-transparent background if needed
    if (m_hasScreenshot) {
        gc->SetBrush(wxBrush(wxColour(0, 0, 0, m_settingBackgroundOpacity)));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRectangle(0, 0, clientSize.GetWidth(), clientSize.GetHeight());

        int centerX = clientSize.GetWidth() / 2;
        int centerY = clientSize.GetHeight() / 2;

        if (m_screenshot.IsOk()) {
            // Use the stored sourceRect (calculated when screenshot was taken)
            wxRect sourceRect = m_screenshotSourceRect;

            // Calculate where the crosshair should be relative to current target position
            // m_screenshotPosition may have been refined during zoom, so calculate offset from sourceRect center
            int sourceRectCenterX = sourceRect.x + sourceRect.width / 2;
            int sourceRectCenterY = sourceRect.y + sourceRect.height / 2;

            // Offset between the current target position and the original screenshot center
            int offsetX = m_screenshotPosition.x - sourceRectCenterX;
            int offsetY = m_screenshotPosition.y - sourceRectCenterY;

            // Calculate crosshair position in display coordinates
            int crosshairX = centerX + offsetX;
            int crosshairY = centerY + offsetY;

            // Create sub-bitmap from the full screenshot
            wxBitmap subBitmap = m_screenshot.GetSubBitmap(sourceRect);

            if (m_isZoomed) {
                // Draw zoomed (magnified) subset
                int zoomedWidth = static_cast<int>(m_settingSelectionWidth * m_settingZoomFactor);
                int zoomedHeight = static_cast<int>(m_settingSelectionHeight * m_settingZoomFactor);
                gc->DrawBitmap(subBitmap,
                              centerX - zoomedWidth / 2,
                              centerY - zoomedHeight / 2,
                              zoomedWidth,
                              zoomedHeight);

                // Draw crosshair at adjusted position (scaled for zoom)
                // The offset is scaled by zoom factor
                int zoomedCrosshairX = centerX + static_cast<int>(offsetX * m_settingZoomFactor);
                int zoomedCrosshairY = centerY + static_cast<int>(offsetY * m_settingZoomFactor);

                gc->SetPen(wxPen(buttonColor, 2));
                wxGraphicsPath path = gc->CreatePath();
                path.MoveToPoint(zoomedCrosshairX - 15, zoomedCrosshairY);
                path.AddLineToPoint(zoomedCrosshairX - 3, zoomedCrosshairY);
                path.MoveToPoint(zoomedCrosshairX + 3, zoomedCrosshairY);
                path.AddLineToPoint(zoomedCrosshairX + 15, zoomedCrosshairY);
                path.MoveToPoint(zoomedCrosshairX, zoomedCrosshairY - 15);
                path.AddLineToPoint(zoomedCrosshairX, zoomedCrosshairY - 3);
                path.MoveToPoint(zoomedCrosshairX, zoomedCrosshairY + 3);
                path.AddLineToPoint(zoomedCrosshairX, zoomedCrosshairY + 15);
                gc->StrokePath(path);
            } else {
                // Draw normal subset with crosshair
                gc->DrawBitmap(subBitmap,
                              centerX - m_settingSelectionWidth / 2,
                              centerY - m_settingSelectionHeight / 2,
                              m_settingSelectionWidth,
                              m_settingSelectionHeight);

                // Draw crosshair at adjusted position (accounts for border clamping)
                gc->SetPen(wxPen(buttonColor, 2));
                wxGraphicsPath path = gc->CreatePath();
                path.MoveToPoint(crosshairX - 15, crosshairY);
                path.AddLineToPoint(crosshairX - 3, crosshairY);
                path.MoveToPoint(crosshairX + 3, crosshairY);
                path.AddLineToPoint(crosshairX + 15, crosshairY);
                path.MoveToPoint(crosshairX, crosshairY - 15);
                path.AddLineToPoint(crosshairX, crosshairY - 3);
                path.MoveToPoint(crosshairX, crosshairY + 3);
                path.AddLineToPoint(crosshairX, crosshairY + 15);
                gc->StrokePath(path);
            }
        }
    }

    // Draw visible buttons with GraphicsContext
    for (auto& button : m_visibleButtons) {
        DrawButtonWithGC(gc, button.get(), buttonColor);
    }

    // Draw gaze cursor (from HeyEyeControl eyepanel.cpp:138-147)
    // When hidden, only show cursor if there's exactly 1 button (UnHide button)
    if (!m_isHiddenMode || m_visibleButtons.size() == 1) {
        if (m_visibleButtons.empty()) {
            const int cursorSize = 80;
            int cursorX = static_cast<int>(m_gazePosition.m_x);
            int cursorY = static_cast<int>(m_gazePosition.m_y);

            gc->SetPen(wxPen(buttonColor, 1));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->DrawEllipse(cursorX - cursorSize/2, cursorY - cursorSize/2, cursorSize, cursorSize);

            if (m_dwellProgress > 0.0f) {
                gc->SetPen(wxPen(buttonColor, 5));
                wxGraphicsPath path = gc->CreatePath();
                path.AddArc(cursorX, cursorY, cursorSize/2, 0, m_dwellProgress * 2.0 * M_PI, true);
                gc->StrokePath(path);
            }
        }
    }

    delete gc;
    mdc.SelectObject(wxNullBitmap);

#ifdef __WXMSW__
    // Use UpdateLayeredWindow for per-pixel alpha transparency
    HWND hwnd = (HWND)GetHWND();
    HDC hdcScreen = ::GetDC(NULL);
    HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = (HBITMAP)bmp.GetHBITMAP();
    HGDIOBJ oldBitmap = ::SelectObject(hdcMem, hBitmap);

    SIZE size = { clientSize.GetWidth(), clientSize.GetHeight() };
    POINT src = { 0, 0 };
    POINT pos = { GetPosition().x, GetPosition().y };

    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    ::UpdateLayeredWindow(hwnd, hdcScreen, &pos, &size, hdcMem, &src, 0, &blend, ULW_ALPHA);

    ::SelectObject(hdcMem, oldBitmap);
    ::DeleteDC(hdcMem);
    ::ReleaseDC(NULL, hdcScreen);
#else
    wxClientDC dc(this);
    dc.DrawBitmap(bmp, 0, 0, true);
#endif
}

// Helper function to draw buttons using GraphicsContext
void EyeOverlay::DrawButtonWithGC(wxGraphicsContext* gc, CircularButton* button, const wxColour& color)
{
    if (!gc || !button) return;

    wxPoint pos = button->GetPosition();
    wxSize size = button->GetSize();
    float progress = button->GetProgress();
    bool isSelected = button->IsSelected();

    // Draw text with GDI+ (handles transparent background correctly)
    wxFont font(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    gc->SetFont(font, color);

    wxString label = button->GetLabel();
    double textWidth, textHeight;
    gc->GetTextExtent(label, &textWidth, &textHeight);
    gc->DrawText(label, pos.x - textWidth/2, pos.y - textHeight/2);

    // Draw circle
    int penWidth = isSelected ? 3 : 1;
    gc->SetPen(wxPen(color, penWidth));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(pos.x - size.GetWidth()/2, pos.y - size.GetHeight()/2, size.GetWidth(), size.GetHeight());

    // Draw progress arc
    if (progress > 0.0f) {
        gc->SetPen(wxPen(color, 6));
        wxGraphicsPath path = gc->CreatePath();
        int reduce = 4;
        path.AddArc(pos.x, pos.y, size.GetWidth()/2 - reduce, 0, progress * 2.0 * M_PI, true);
        gc->StrokePath(path);
    }
}

void EyeOverlay::OnEraseBackground(wxEraseEvent& event)
{
    // With wxAutoBufferedPaintDC, we handle clearing in OnPaint
    // Do nothing here to prevent flickering
    wxUnusedVar(event);
}

void EyeOverlay::OnSize(wxSizeEvent& event)
{
    event.Skip();
    UpdateButtonPositions();
}

void EyeOverlay::OnKeyDown(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();

    switch (keyCode) {
        case 'K':
        case 'k':
            ShowKeyboard(!m_keyboardVisible);
            break;

        case 'M':
        case 'm':
            OnToggleInputMode(wxCommandEvent());
            break;

        case WXK_ESCAPE:
            Close(true);  // Force close
            break;

        default:
            event.Skip();
    }
}

void EyeOverlay::OnClose(wxCloseEvent& event)
{
    wxLogMessage("EyeOverlay: Closing application...");

    // Stop gaze tracking to halt the 120Hz timer
    if (m_gazeTracker) {
        m_gazeTracker->StopTracking();
    }

    // Clear all buttons and resources
    ClearAllButtons();

    // Destroy the window and exit the application
    Destroy();
}

void EyeOverlay::OnGazePositionUpdated(float x, float y, uint64_t timestamp)
{
    wxPoint2DDouble oldPosition = m_gazePosition;
    m_gazePosition = wxPoint2DDouble(x, y);
    m_lastGazeTimestamp = timestamp;

    // Calculate delta time
    float deltaTime = 0.0f;
    if (m_previousTimestamp > 0) {
        deltaTime = static_cast<float>(timestamp - m_previousTimestamp);
    }
    m_previousTimestamp = timestamp;

    // Update keyboard with gaze position if visible
    if (m_keyboard && m_keyboardVisible) {
        m_keyboard->UpdateGazePosition(x, y);
        return;
    }

    // Handle scroll mode (from HeyEyeControl eyepanel.cpp:383-410)
    if (m_isScrollMode) {
        wxSize clientSize = GetClientSize();
        int centerY = clientSize.GetHeight() / 2;

        // Calculate vertical difference from screen center
        float diff = y - centerY;

        // If gaze is far enough from center (0.7 * half_height), trigger scroll
        float threshold = 0.7f * (clientSize.GetHeight() / 2.0f);

        if (std::abs(diff) >= threshold) {
#ifdef __WXMSW__
            INPUT input;
            ZeroMemory(&input, sizeof(input));

            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_WHEEL;

            // Scroll up if above center (negative diff), down if below (positive diff)
            if (diff < 0) {
                input.mi.mouseData = +20;  // Scroll up
            } else {
                input.mi.mouseData = -20;  // Scroll down
            }

            SendInput(1, &input, sizeof(INPUT));
            wxLogMessage("Scroll: diff=%.1f, direction=%s", diff, (diff < 0) ? "UP" : "DOWN");
#endif
        }

        // Continue processing (don't return early) to allow button interaction
    }

    // Track if visual state changed (requires refresh)
    bool needsRefresh = false;

    // Check if hovering over any visible button
    bool onButton = false;
    for (auto& button : m_visibleButtons) {
        if (button->IsPointInside(x, y)) {
            if (button->UpdateProgress(deltaTime, m_settingHoldTime)) {
                needsRefresh = true;
            }
            onButton = true;
        } else {
            if (button->ResetProgress()) {
                needsRefresh = true;
            }
        }
    }

    // Handle hidden mode special logic (from HeyEyeControl eyepanel.cpp:411-486)
    if (m_isHiddenMode) {
        // If there's an UnHide button and gaze moves far below it, remove it (eyepanel.cpp:413-415)
        if (m_visibleButtons.size() == 1) {
            auto& button = m_visibleButtons[0];
            float buttonBottom = button->GetPosition().y + button->GetSize().GetHeight();
            if (y > buttonBottom + 2 * button->GetSize().GetHeight()) {
                m_visibleButtons.clear();
                needsRefresh = true;
            }
        }

        // If looking at top of screen and no buttons, create UnHide button (eyepanel.cpp:468-477)
        if (m_visibleButtons.empty() && y <= 0) {
            wxSize clientSize = GetClientSize();
            int centerX = clientSize.GetWidth() / 2;
            int centerY = clientSize.GetHeight() / 2;

            auto btnUnHide = std::make_unique<CircularButton>(wxT("UnHide"), wxPoint(centerX, centerY - 400));
            btnUnHide->OnActivated = [this]() {
                ToggleHide();
            };
            m_visibleButtons.push_back(std::move(btnUnHide));
            needsRefresh = true;
        }
    }

    // If not on any button, handle dwell detection (only when NOT hidden - eyepanel.cpp:488)
    if (!onButton && m_visibleButtons.empty() && !m_isHiddenMode) {
        // Only refresh if dwell state actually changed
        if (UpdateDwellDetection(x, y, timestamp)) {
            needsRefresh = true;
        }

        // Refresh when gaze cursor moves significantly (>5 pixels to reduce 120Hz to ~20-30Hz)
        float dx = m_gazePosition.m_x - oldPosition.m_x;
        float dy = m_gazePosition.m_y - oldPosition.m_y;
        float distanceMoved = std::sqrt(dx * dx + dy * dy);
        if (distanceMoved > 5.0f) {
            needsRefresh = true;
        }
    }

    // Periodically ensure window stays on top, but ONLY when no buttons are visible
    // This keeps the dwell circle on top of context menus, but avoids interfering
    // with button selection when the radial menu is open
    if (m_visibleButtons.empty() && (timestamp - m_lastBringToFrontTimestamp) >= 100000) {  // 100ms
        EnsureOnTop();
        m_lastBringToFrontTimestamp = timestamp;
    }

    // Only refresh when visual state changed
    if (needsRefresh) {
        Refresh(false);  // false = don't erase background (we handle it in OnPaint with buffered DC)
        // Note: Update() removed - let wxWidgets batch paint events naturally
    }
}

void EyeOverlay::OnLetterSelected(wxChar letter)
{
    wxLogMessage("Letter selected: %c", letter);
    if (m_textEngine) {
        m_textEngine->AppendCharacter(letter);
    }
}

void EyeOverlay::OnSwipeCompleted(const std::vector<std::pair<float, float>>& path)
{
    wxLogMessage("Swipe completed with %zu points", path.size());

    if (!m_textEngine) return;

    // Predict word
    wxString prediction = m_textEngine->PredictFromSwipe(path);

    if (!prediction.IsEmpty()) {
        m_textEngine->AppendText(prediction + wxT(" "));
        wxLogMessage("Predicted: %s", prediction);
    }

    // Clear the swipe path
    if (m_keyboard) {
        m_keyboard->ClearSwipePath();
    }
}

void EyeOverlay::OnSpacePressed()
{
    wxLogMessage("Space pressed");
    if (m_textEngine) {
        m_textEngine->AppendCharacter(wxT(' '));
    }
}

void EyeOverlay::OnTextChanged(const wxString& text)
{
    // Text display removed - no longer using wxStaticText overlay
    // Text is handled by the keyboard component
    wxLogMessage("Text changed: %s", text);
}

void EyeOverlay::OnToggleInputMode(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (!m_keyboard) return;

    InputMode currentMode = m_keyboard->GetInputMode();
    InputMode newMode = (currentMode == InputMode::LetterByLetter)
                        ? InputMode::Swipe
                        : InputMode::LetterByLetter;

    m_keyboard->SetInputMode(newMode);

    wxString modeText = (newMode == InputMode::LetterByLetter)
                       ? wxT("Mode: Letter-by-Letter")
                       : wxT("Mode: Swipe");

    wxLogMessage("%s", modeText);
}

void EyeOverlay::OnToggleKeyboard(wxCommandEvent& event)
{
    wxUnusedVar(event);
    ShowKeyboard(!m_keyboardVisible);
    wxLogMessage("Keyboard visible: %d", m_keyboardVisible);
}

void EyeOverlay::OnDeleteLastWord(wxCommandEvent& event)
{
    wxUnusedVar(event);
    if (m_textEngine) {
        m_textEngine->DeleteLastWord();
    }
}

void EyeOverlay::OnSpeak(wxCommandEvent& event)
{
    wxUnusedVar(event);
    // TODO: Integrate espeak-ng for text-to-speech
    wxLogMessage("Speak: %s", m_textEngine->GetCurrentText());
}

void EyeOverlay::SetupUI()
{
    // Create keyboard (hidden by default, positioned manually)
    m_keyboard = new KeyboardView(this);
    m_keyboard->Show(false);

    // Set up keyboard callbacks
    m_keyboard->OnLetterSelected = [this](wxChar letter) {
        OnLetterSelected(letter);
    };
    m_keyboard->OnSwipeCompleted = [this](const std::vector<std::pair<float, float>>& path) {
        OnSwipeCompleted(path);
    };
    m_keyboard->OnSpacePressed = [this]() {
        OnSpacePressed();
    };

    // Position keyboard at bottom of screen
    wxDisplay display(wxDisplay::GetFromPoint(wxGetMousePosition()));
    wxRect screenRect = display.GetGeometry();
    m_keyboard->SetSize(screenRect.GetWidth(), 400);
    m_keyboard->SetPosition(wxPoint(0, screenRect.GetHeight() - 400));
}

void EyeOverlay::UpdateButtonPositions()
{
    // No persistent buttons to update (keyboard button is now in the radial panel)
}

void EyeOverlay::CreateButtonsAtCenter()
{
    // Clear any existing buttons
    m_visibleButtons.clear();
    m_dwellProgress = 0.0f;
    m_positionHistory.clear();
    m_timestampHistory.clear();

    // Always bring window to front when creating buttons
    EnsureOnTop();

    // Only take screenshot if we don't have one yet (first time or after undo)
    if (!m_hasScreenshot) {
        // Save the screenshot position (where gaze was when dwell completed)
        m_screenshotPosition = wxPoint(
            static_cast<int>(m_gazePosition.m_x),
            static_cast<int>(m_gazePosition.m_y)
        );

        // Make overlay invisible before screenshot (like HeyEyeControl - just set flag and repaint)
        m_visible = false;
        Refresh();

        // Take screenshot of ENTIRE screen (like HeyEyeControl)
        wxScreenDC screenDC;
        wxSize clientSize = GetClientSize();
        m_screenshot = wxBitmap(clientSize.GetWidth(), clientSize.GetHeight());
        wxMemoryDC memDC(m_screenshot);

        memDC.Blit(
            0, 0,
            clientSize.GetWidth(), clientSize.GetHeight(),
            &screenDC,
            0, 0
        );

        memDC.SelectObject(wxNullBitmap);
        m_hasScreenshot = true;

        // Calculate and store the actual screenshot area (with border clamping)
        int intendedX = m_screenshotPosition.x - m_settingSelectionWidth / 2;
        int intendedY = m_screenshotPosition.y - m_settingSelectionHeight / 2;

        m_screenshotSourceRect = wxRect(
            intendedX,
            intendedY,
            m_settingSelectionWidth,
            m_settingSelectionHeight
        );

        // Clamp to screen bounds (same logic as in OnPaint)
        if (m_screenshotSourceRect.x < 0)
            m_screenshotSourceRect.x = 0;
        if (m_screenshotSourceRect.y < 0)
            m_screenshotSourceRect.y = 0;
        if (m_screenshotSourceRect.x + m_screenshotSourceRect.width > m_screenshot.GetWidth())
            m_screenshotSourceRect.x = m_screenshot.GetWidth() - m_screenshotSourceRect.width;
        if (m_screenshotSourceRect.y + m_screenshotSourceRect.height > m_screenshot.GetHeight())
            m_screenshotSourceRect.y = m_screenshot.GetHeight() - m_screenshotSourceRect.height;

        // Make overlay visible again
        m_visible = true;
    }

    // Create buttons at center of screen
    wxSize clientSize = GetClientSize();
    int centerX = clientSize.GetWidth() / 2;
    int centerY = clientSize.GetHeight() / 2;

    // Create buttons in radial pattern (matching HeyEyeControl layout)
    // All button positions are relative to window center

    // Keyboard button (top-left)
    auto btnKeyboard = std::make_unique<CircularButton>(
        m_keyboardVisible ? wxT("Hide\nKeyboard") : wxT("Show\nKeyboard"),
        wxPoint(centerX - 175, centerY - 175)
    );
    btnKeyboard->OnActivated = [this]() {
        ShowKeyboard(!m_keyboardVisible);
        ClearAllButtons();
    };
    m_visibleButtons.push_back(std::move(btnKeyboard));

    // Undo button (left)
    auto btnUndo = std::make_unique<CircularButton>(wxT("Undo"), wxPoint(centerX - 250, centerY));
    btnUndo->OnActivated = [this]() {
        ClearAllButtons();
    };
    m_visibleButtons.push_back(std::move(btnUndo));

    // Zoom button (top)
    auto btnZoom = std::make_unique<CircularButton>(wxT("Zoom"), wxPoint(centerX, centerY - 250));
    btnZoom->OnActivated = [this]() {
        wxLogMessage("Zoom activated");
        m_isZoomed = true;
        m_visibleButtons.clear();  // Clear buttons to show zoomed view
    };
    m_visibleButtons.push_back(std::move(btnZoom));

    // Button layout depends on drag mode (from HeyEyeControl eyepanel.cpp:544-593)
    if (m_isDragMode) {
        // In drag mode: only show Drop button (plus Undo and Zoom which are always shown)
        auto btnDrop = std::make_unique<CircularButton>(wxT("Drop"), wxPoint(centerX + 225, centerY + 250));
        btnDrop->OnActivated = [this]() {
            Drop();
        };
        m_visibleButtons.push_back(std::move(btnDrop));
    } else {
        // Normal mode: show all buttons

        // Scroll button (right) - shows selected state if scroll mode is active (from HeyEyeControl eyepanel.cpp:558-562)
        auto btnScroll = std::make_unique<CircularButton>(wxT("Scroll"), wxPoint(centerX + 250, centerY));
        btnScroll->SetSelected(m_isScrollMode);  // Show current scroll mode state
        btnScroll->OnActivated = [this]() {
            ToggleScroll();
        };
        m_visibleButtons.push_back(std::move(btnScroll));

        // Click button (bottom-left of center)
        auto btnClick = std::make_unique<CircularButton>(wxT("Click"), wxPoint(centerX - 75, centerY + 250));
        btnClick->OnActivated = [this]() {
            Click();
        };
        m_visibleButtons.push_back(std::move(btnClick));

        // Click Right button (bottom-right of center)
        auto btnClickRight = std::make_unique<CircularButton>(wxT("Click\nRight"), wxPoint(centerX + 75, centerY + 250));
        btnClickRight->OnActivated = [this]() {
            ClickRight();
        };
        m_visibleButtons.push_back(std::move(btnClickRight));

        // Drag button (bottom-far-right)
        auto btnDrag = std::make_unique<CircularButton>(wxT("Drag"), wxPoint(centerX + 225, centerY + 250));
        btnDrag->OnActivated = [this]() {
            Drag();
        };
        m_visibleButtons.push_back(std::move(btnDrag));

        // Double Click button (bottom-far-left)
        auto btnDoubleClick = std::make_unique<CircularButton>(wxT("Double\nClick"), wxPoint(centerX - 225, centerY + 250));
        btnDoubleClick->OnActivated = [this]() {
            DoubleClick();
        };
        m_visibleButtons.push_back(std::move(btnDoubleClick));

        // Hide button (top-center, higher)
        auto btnHide = std::make_unique<CircularButton>(wxT("Hide"), wxPoint(centerX, centerY - 400));
        btnHide->OnActivated = [this]() {
            ToggleHide();
        };
        m_visibleButtons.push_back(std::move(btnHide));

        // Quit button (top-right)
        auto btnQuit = std::make_unique<CircularButton>(wxT("Quit"), wxPoint(centerX + 250, centerY - 250));
        btnQuit->OnActivated = [this]() {
            wxLogMessage("Quit button activated");
            Close(true);  // Force close
        };
        m_visibleButtons.push_back(std::move(btnQuit));
    }
}

void EyeOverlay::ClearAllButtons()
{
    m_visibleButtons.clear();
    m_dwellProgress = 0.0f;
    m_positionHistory.clear();
    m_timestampHistory.clear();
    m_hasScreenshot = false;
    m_screenshot = wxNullBitmap;
    m_screenshotSourceRect = wxRect(0, 0, 0, 0);
    m_isZoomed = false;
    // Note: m_isScrollMode and m_isDragMode are NOT cleared here - they persist until explicitly changed
}

void EyeOverlay::Click()
{
    wxLogMessage("Click: Performing left click at position (%d, %d)", m_screenshotPosition.x, m_screenshotPosition.y);

    m_isScrollMode = false;

    m_visible = false;
    Hide();
    Refresh();
    Update();

#ifdef __WXMSW__
    wxMilliSleep(10);

    // Move cursor and click
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(10);

    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, inputs, sizeof(INPUT));
#endif

    m_visible = true;

    // Ensure overlay stays on top after interaction
    EnsureOnTop();

    ClearAllButtons();
}

void EyeOverlay::ClickRight()
{
    wxLogMessage("ClickRight: Performing right click at position (%d, %d)", m_screenshotPosition.x, m_screenshotPosition.y);

    m_isScrollMode = false;

    m_visible = false;
    Hide();
    Refresh();
    Update();

#ifdef __WXMSW__
    wxMilliSleep(10);

    // Move cursor and click
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);
    
    wxMilliSleep(10);

    // do a left click at the location before
    INPUT inputs_1[2];
    ZeroMemory(inputs_1, sizeof(inputs_1));
    inputs_1[0].type = INPUT_MOUSE;
    inputs_1[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs_1[1].type = INPUT_MOUSE;
    inputs_1[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    INPUT inputs_2[2];
    ZeroMemory(inputs_2, sizeof(inputs_2));
    inputs_2[0].type = INPUT_MOUSE;
    inputs_2[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    inputs_2[1].type = INPUT_MOUSE;
    inputs_2[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(2, inputs_1, sizeof(INPUT));
    wxMilliSleep(10);
    SendInput(2, inputs_2, sizeof(INPUT));
#endif

    m_visible = true;

    // After right-click, ensure overlay stays on top (context menu may have taken topmost position)
    EnsureOnTop();

    ClearAllButtons();
}

void EyeOverlay::DoubleClick()
{
    wxLogMessage("DoubleClick: Performing double click at position (%d, %d)", m_screenshotPosition.x, m_screenshotPosition.y);

    m_isScrollMode = false;

    m_visible = false;
    Hide();
    Refresh();
    Update();

#ifdef __WXMSW__
    wxMilliSleep(10);

    // Move cursor and double click
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(10);

    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    // Send twice for double click
    SendInput(2, inputs, sizeof(INPUT));
    wxMilliSleep(50);
    SendInput(2, inputs, sizeof(INPUT));
#endif

    m_visible = true;

    // Ensure overlay stays on top after interaction
    EnsureOnTop();

    ClearAllButtons();
}

void EyeOverlay::ToggleScroll()
{
    // Toggle scroll mode (from HeyEyeControl eyepanel.cpp:343-348)
    m_isScrollMode = !m_isScrollMode;
    wxLogMessage("Scroll mode: %s", m_isScrollMode ? "ON" : "OFF");

    // Clear all buttons when toggling scroll mode
    ClearAllButtons();
}

void EyeOverlay::Drag()
{
    wxLogMessage("Drag: Starting drag at position (%d, %d)", m_screenshotPosition.x, m_screenshotPosition.y);

    m_isDragMode = true;
    m_isScrollMode = false;

    m_visible = false;
    Hide();
    Refresh();
    Update();

#ifdef __WXMSW__
    wxMilliSleep(10);

    // Move cursor and start drag (button DOWN only)
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);
    
    wxMilliSleep(10);

    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
#endif

    m_visible = true;

    // Ensure overlay stays on top after interaction
    EnsureOnTop();

    ClearAllButtons();
}

void EyeOverlay::Drop()
{
    wxLogMessage("Drop: Releasing drag at position (%d, %d)", m_screenshotPosition.x, m_screenshotPosition.y);

    m_isDragMode = false;

    m_visible = false;
    Hide();
    Refresh();
    Update();

#ifdef __WXMSW__
    wxMilliSleep(10);

    // Move cursor and release drag (button UP)
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(10);

    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
#endif

    m_visible = true;

    // Ensure overlay stays on top after interaction
    EnsureOnTop();

    ClearAllButtons();
}

void EyeOverlay::ToggleHide()
{
    // Toggle hidden mode (from HeyEyeControl eyepanel.cpp:318-325)
    m_isHiddenMode = !m_isHiddenMode;
    wxLogMessage("Hidden mode: %s", m_isHiddenMode ? "ON" : "OFF");

    // Disable scroll and drag modes when toggling hide (from HeyEyeControl eyepanel.cpp:322-323)
    m_isScrollMode = false;
    m_isDragMode = false;

    // Clear all buttons when toggling hide mode
    ClearAllButtons();
}

void EyeOverlay::EnsureOnTop()
{
#ifdef __WXMSW__
    // Bring window to topmost Z-order position
    // This is needed because context menus and other windows can take over the topmost position
    // Similar to HeyEyeControl eyepanel.cpp:501-503
    HWND hwnd = (HWND)GetHWND();
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
#endif
}

bool EyeOverlay::UpdateDwellDetection(float x, float y, uint64_t timestamp)
{
    // Add current position to history
    m_positionHistory.push_back(wxPoint2DDouble(x, y));
    m_timestampHistory.push_back(timestamp);

    // Remove old positions (older than wait time)
    while (!m_timestampHistory.empty() &&
           (timestamp - m_timestampHistory.front()) >= static_cast<uint64_t>(m_settingWaitTime * 1000)) {
        m_positionHistory.erase(m_positionHistory.begin());
        m_timestampHistory.erase(m_timestampHistory.begin());
    }

    // Check if movement is stable (within 30 pixels like HeyEyeControl)
    if (!m_positionHistory.empty()) {
        float minX = 1e15f, maxX = 0.0f, minY = 1e15f, maxY = 0.0f;
        for (const auto& pt : m_positionHistory) {
            if (pt.m_x < minX) minX = pt.m_x;
            if (pt.m_x > maxX) maxX = pt.m_x;
            if (pt.m_y < minY) minY = pt.m_y;
            if (pt.m_y > maxY) maxY = pt.m_y;
        }

        bool isStable = ((maxX - minX) < 30) && ((maxY - minY) < 30);

        if (isStable && m_timestampHistory.size() > 2) {
            // Update progress
            float oldProgress = m_dwellProgress;
            float deltaT = static_cast<float>(m_timestampHistory.back() - m_timestampHistory[m_timestampHistory.size() - 2]);
            m_dwellProgress += deltaT / (m_settingHoldTime * 1000.0f);

            if (m_dwellProgress >= 1.0f) {
                // Dwell complete
                m_dwellProgress = 0.0f;

                if (m_isZoomed) {
                    // In zoom mode - refine position and exit zoom
                    wxLogMessage("Zoom refinement: refining position from (%d, %d)", m_screenshotPosition.x, m_screenshotPosition.y);

                    wxSize clientSize = GetClientSize();
                    int centerX = clientSize.GetWidth() / 2;
                    int centerY = clientSize.GetHeight() / 2;

                    // In zoom view, screen center corresponds to sourceRect center
                    // User is looking at (x, y) which corresponds to a specific point in the screenshot
                    int sourceRectCenterX = m_screenshotSourceRect.x + m_screenshotSourceRect.width / 2;
                    int sourceRectCenterY = m_screenshotSourceRect.y + m_screenshotSourceRect.height / 2;

                    // Calculate the absolute position in screen coordinates where user is looking
                    m_screenshotPosition.x = sourceRectCenterX + static_cast<int>((x - centerX) / m_settingZoomFactor);
                    m_screenshotPosition.y = sourceRectCenterY + static_cast<int>((y - centerY) / m_settingZoomFactor);

                    wxLogMessage("Zoom refinement: new position (%d, %d)", m_screenshotPosition.x, m_screenshotPosition.y);

                    // Exit zoom mode and recreate buttons
                    m_isZoomed = false;
                    m_dwellPosition = m_gazePosition;
                    CreateButtonsAtCenter();
                } else {
                    // Normal mode - create buttons at center
                    wxLogMessage("DWELL COMPLETE! Creating buttons at center...");
                    m_dwellPosition = m_gazePosition;
                    CreateButtonsAtCenter();
                }

                return true;  // Visual state changed
            }

            // Only refresh if progress changed significantly (every 5%)
            return (static_cast<int>(m_dwellProgress * 20) != static_cast<int>(oldProgress * 20));
        } else {
            if (m_dwellProgress > 0.0f) {
                m_dwellProgress = 0.0f;
                return true;  // Visual state changed (progress reset)
            }
        }
    }

    return false;  // No visual change
}
