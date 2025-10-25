#include "eyeoverlay.h"
#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/dcscreen.h>
#include <wx/rawbmp.h>
#include <wx/graphics.h>
#include <cmath>      // For std::sqrt
#include <map>        // For std::map
#include <cctype>     // For toupper
#include <algorithm>  // For std::find_if

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
    , m_settings(nullptr)
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
    , m_isHiddenMode(true)  // Start in hidden mode by default
    , m_dwellProgress(0.0f)
    , m_dwellPosition(0, 0)
    , m_lastBringToFrontTimestamp(0)
    , m_settingWaitTime(800)
    , m_settingHoldTime(800)
    , m_settingCursorDelay(50)
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
    // Load settings from config file
    m_settings = new Settings();

    // Apply settings
    m_settingWaitTime = m_settings->GetWaitTime();
    m_settingHoldTime = m_settings->GetHoldTime();
    m_settingCursorDelay = m_settings->GetCursorDelay();
    m_settingZoomFactor = m_settings->GetZoomFactor();
    m_settingBackgroundOpacity = m_settings->GetBackgroundOpacity();
    m_settingsColorR = m_settings->GetColorR();
    m_settingsColorG = m_settings->GetColorG();
    m_settingsColorB = m_settings->GetColorB();
    m_settingSelectionWidth = m_settings->GetSelectionWidth();
    m_settingSelectionHeight = m_settings->GetSelectionHeight();
    // Transparent background setup
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxBLACK);

#ifdef __WXMSW__
    // Enable layered window for per-pixel alpha transparency
    // Also add WS_EX_NOACTIVATE to prevent window from stealing focus (like HeyEyeControl Qt::WA_ShowWithoutActivating)
    // Also add WS_EX_TRANSPARENT to let click pass throw the window
    HWND hwnd = (HWND)GetHWND();
    LONG exStyle = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    ::SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_NOACTIVATE); // | WS_EX_TRANSPARENT

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

EyeOverlay::~EyeOverlay()
{
    // Save settings before exit
    if (m_settings) {
        // Update settings with current values
        m_settings->SetWaitTime(m_settingWaitTime);
        m_settings->SetHoldTime(m_settingHoldTime);
        m_settings->SetZoomFactor(m_settingZoomFactor);
        m_settings->SetBackgroundOpacity(m_settingBackgroundOpacity);
        m_settings->SetColor(m_settingsColorR, m_settingsColorG, m_settingsColorB);
        m_settings->SetSelectionWidth(m_settingSelectionWidth);
        m_settings->SetSelectionHeight(m_settingSelectionHeight);

        // Save to file
        m_settings->Save();

        delete m_settings;
        m_settings = nullptr;
    }
}

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

    // Don't use the separate keyboard window - we'll draw it on the overlay instead
    // if (m_keyboard) {
    //     m_keyboard->Show(show);
    // }

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

    // Clear to fully transparent first
    gc->SetCompositionMode(wxCOMPOSITION_CLEAR);
    gc->DrawRectangle(0, 0, clientSize.GetWidth(), clientSize.GetHeight());

    // Set high-quality rendering for transparent backgrounds
    gc->SetCompositionMode(wxCOMPOSITION_OVER);  // Alpha blending mode
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);  // Enable antialiasing
    gc->SetInterpolationQuality(wxINTERPOLATION_BEST);  // High-quality scaling

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

    // Draw semi-transparent background if buttons/keyboard are visible (provides proper background for text antialiasing)
    if (m_hasScreenshot || !m_visibleButtons.empty() || m_keyboardVisible) {
        gc->SetBrush(wxBrush(wxColour(0, 0, 0, m_settingBackgroundOpacity)));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRectangle(0, 0, clientSize.GetWidth(), clientSize.GetHeight());
    }

    // Draw screenshot if available
    if (m_hasScreenshot) {

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

    // Draw visible buttons with GraphicsContext (only if keyboard not visible)
    if (!m_keyboardVisible) {
        for (auto& button : m_visibleButtons) {
            DrawButtonWithGC(gc, button.get(), buttonColor);
        }
    }

    // Draw keyboard if visible (on the same layer as buttons)
    if (m_keyboardVisible) {
        DrawKeyboardWithGC(gc, buttonColor);
    }

    // Draw gaze cursor (from HeyEyeControl eyepanel.cpp:138-147)
    // When hidden, only show cursor if there's exactly 1 button (UnHide button)
    // Show cursor at all times (even when buttons/keyboard visible) for continuous feedback
    if (!m_isHiddenMode || m_visibleButtons.size() == 1 || m_keyboardVisible) {
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
    // Antialiasing and quality settings are set globally in OnPaint
    wxFont font(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    gc->SetFont(font, color);

    wxString label = button->GetLabel();
    double textWidth, textHeight;
    gc->GetTextExtent(label, &textWidth, &textHeight);

    // Draw text - GDI+ will handle antialiasing correctly on transparent background
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

// Helper function to draw keyboard using GraphicsContext (AZERTY layout)
void EyeOverlay::DrawKeyboardWithGC(wxGraphicsContext* gc, const wxColour& color)
{
    if (!gc) return;

    wxSize clientSize = GetClientSize();

    // Draw text display area at top center
    int textBoxWidth = 800;
    int textBoxHeight = 80;
    int textBoxX = (clientSize.GetWidth() - textBoxWidth) / 2;
    int textBoxY = 50;  // Position at top center

    // Draw white background box
    gc->SetBrush(wxBrush(wxColour(255, 255, 255, 230)));  // Semi-transparent white
    gc->SetPen(wxPen(color, 2));
    gc->DrawRoundedRectangle(textBoxX, textBoxY, textBoxWidth, textBoxHeight, 10);

    // Draw the current text inside the box
    if (m_textEngine) {
        wxString currentText = m_textEngine->GetCurrentText();
        if (currentText.IsEmpty()) {
            currentText = wxT("Type here...");  // Placeholder text
        }

        wxFont textFont(20, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        gc->SetFont(textFont, wxColour(0, 0, 0));  // Black text

        double textWidth, textHeight;
        gc->GetTextExtent(currentText, &textWidth, &textHeight);

        // Center text vertically, align left with some padding
        int textPadding = 20;
        gc->DrawText(currentText, textBoxX + textPadding, textBoxY + (textBoxHeight - textHeight) / 2);
    }

    // Save previous progress values before clearing
    std::map<wxString, float> progressMap;
    for (const auto& key : m_keyboardKeys) {
        progressMap[key.label] = key.dwellProgress;
    }

    // Clear previous keyboard keys (will rebuild on each draw)
    m_keyboardKeys.clear();

    // Keyboard positioned at bottom-center of screen
    int keyboardWidth = 1200;
    int keyboardHeight = 400;
    int startX = (clientSize.GetWidth() - keyboardWidth) / 2;
    int startY = clientSize.GetHeight() - keyboardHeight - 50;

    // Key size (matching HeyEyeTracker SIZE = 20.0f, scaled up for visibility)
    const float SCALE = 4.0f;  // Scale factor for visibility
    const float BASE_SIZE = 20.0f;  // HeyEyeTracker original SIZE
    const float SIZE = BASE_SIZE * SCALE;  // Scaled SIZE for display

    // AZERTY layout (from HeyEyeTracker ranking_features.cpp)
    // Rows 1, 2, 3 from lines_down[] (we skip row 0 which has numbers/special chars)
    const std::vector<std::string> layout = {
        "azertyuiop",    // Row 1: azertyuiop^$ (skip ^$ for simplicity)
        "qsdfghjklm",    // Row 2: qsdfghjklm* (skip * for simplicity)
        "wxcvbn,;:!",    // Row 3: <wxcvbn,;:! (skip < for simplicity)
    };

    wxFont font(18, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    gc->SetFont(font, color);

    // Draw letter keys using HeyEyeTracker position formula
    for (size_t l = 0; l < layout.size(); ++l) {
        const std::string& line = layout[l];
        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];

            // Calculate position using HeyEyeTracker formula
            // In HeyEyeTracker: x = (i + fmod(0.5 * l, 1.5)) * BASE_SIZE
            //                   y = 90.0 - BASE_SIZE * l
            // Our layout array: layout[0] = row 1, layout[1] = row 2, layout[2] = row 3
            int heyeyeRow = l + 1;  // Convert to HeyEyeTracker row number (1, 2, 3)

            float keyX = startX + (i + fmod(0.5f * heyeyeRow, 1.5f)) * SIZE;

            // Y coordinate: In HeyEyeTracker, y = 90.0 - BASE_SIZE * row
            // Higher y = top, lower y = bottom
            // On screen, higher Y = bottom, lower Y = top
            // So we need to map: HeyEyeTracker y to screen offset
            // Offset from reference (y=90.0) = 90.0 - y = BASE_SIZE * row
            float keyY = startY + heyeyeRow * SIZE;

            // Create label - uppercase for letters, as-is for punctuation
            char displayChar = isalpha(c) ? toupper(c) : c;
            wxString keyLabel = wxString::Format("%c", displayChar);

            // Store key bounds for hit testing
            int keySize = SIZE * 0.8f;  // Slightly smaller than spacing for visual separation
            m_keyboardKeys.push_back(KeyboardKey(keyLabel, wxRect(keyX - keySize/2, keyY - keySize/2, keySize, keySize)));

            // Restore previous progress if it exists
            if (progressMap.count(keyLabel) > 0) {
                m_keyboardKeys.back().dwellProgress = progressMap[keyLabel];
            }

            // Draw key rectangle
            gc->SetPen(wxPen(color, 2));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->DrawRoundedRectangle(keyX - keySize/2, keyY - keySize/2, keySize, keySize, 10);

            // Draw key label
            double textWidth, textHeight;
            gc->GetTextExtent(keyLabel, &textWidth, &textHeight);
            gc->DrawText(keyLabel, keyX - textWidth/2, keyY - textHeight/2);

            // Draw progress arc if this key has dwell progress
            float progress = m_keyboardKeys.back().dwellProgress;
            if (progress > 0.0f) {
                gc->SetPen(wxPen(color, 6));
                wxGraphicsPath path = gc->CreatePath();
                path.AddArc(keyX, keyY, keySize/2 - 5, 0, progress * 2.0 * M_PI, true);
                gc->StrokePath(path);
            }
        }
    }

    // Add space bar (at position matching HeyEyeTracker)
    // Space: keyboard_coord[' '] = std::make_pair(100.0f, 90.0f - BASE_SIZE * 4);
    // HeyEyeTracker y = 10.0, offset from top = 90.0 - 10.0 = 80.0
    float spaceX = startX + 100.0f * SCALE;
    float spaceY = startY + 4 * SIZE;  // 4 rows down from reference
    int spaceWidth = SIZE * 3;  // Make space bar wider
    int spaceHeight = SIZE * 0.8f;

    m_keyboardKeys.push_back(KeyboardKey(wxT("SPACE"), wxRect(spaceX - spaceWidth/2, spaceY - spaceHeight/2, spaceWidth, spaceHeight)));
    if (progressMap.count(wxT("SPACE")) > 0) {
        m_keyboardKeys.back().dwellProgress = progressMap[wxT("SPACE")];
    }

    gc->SetPen(wxPen(color, 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(spaceX - spaceWidth/2, spaceY - spaceHeight/2, spaceWidth, spaceHeight, 10);

    double textWidth, textHeight;
    gc->GetTextExtent(wxT("SPACE"), &textWidth, &textHeight);
    gc->DrawText(wxT("SPACE"), spaceX - textWidth/2, spaceY - textHeight/2);

    float spaceProgress = m_keyboardKeys.back().dwellProgress;
    if (spaceProgress > 0.0f) {
        gc->SetPen(wxPen(color, 6));
        wxGraphicsPath path = gc->CreatePath();
        path.AddArc(spaceX, spaceY, spaceHeight/2 - 5, 0, spaceProgress * 2.0 * M_PI, true);
        gc->StrokePath(path);
    }

    // Add backspace key (right side, aligned with row 1)
    // Row 1 offset = 1 * SIZE
    float backspaceX = startX + SIZE * 12;
    float backspaceY = startY + 1 * SIZE;  // Aligned with row 1 (azertyuiop)
    int backspaceSize = SIZE * 0.8f;

    m_keyboardKeys.push_back(KeyboardKey(wxT("⌫"), wxRect(backspaceX - backspaceSize/2, backspaceY - backspaceSize/2, backspaceSize, backspaceSize)));
    if (progressMap.count(wxT("⌫")) > 0) {
        m_keyboardKeys.back().dwellProgress = progressMap[wxT("⌫")];
    }

    gc->SetPen(wxPen(color, 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(backspaceX - backspaceSize/2, backspaceY - backspaceSize/2, backspaceSize, backspaceSize, 10);

    gc->GetTextExtent(wxT("⌫"), &textWidth, &textHeight);
    gc->DrawText(wxT("⌫"), backspaceX - textWidth/2, backspaceY - textHeight/2);

    float backspaceProgress = m_keyboardKeys.back().dwellProgress;
    if (backspaceProgress > 0.0f) {
        gc->SetPen(wxPen(color, 6));
        wxGraphicsPath path = gc->CreatePath();
        path.AddArc(backspaceX, backspaceY, backspaceSize/2 - 5, 0, backspaceProgress * 2.0 * M_PI, true);
        gc->StrokePath(path);
    }

    // Draw Undo button (top-left corner)
    int undoX = 50;
    int undoY = 50;
    int undoSize = 100;

    gc->SetPen(wxPen(color, 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(undoX - undoSize/2, undoY - undoSize/2, undoSize, undoSize);

    wxFont undoFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    gc->SetFont(undoFont, color);
    gc->GetTextExtent(wxT("Undo"), &textWidth, &textHeight);
    gc->DrawText(wxT("Undo"), undoX - textWidth/2, undoY - textHeight/2);

    // Store Undo button as a special key
    m_keyboardKeys.push_back(KeyboardKey(wxT("UNDO"), wxRect(undoX - undoSize/2, undoY - undoSize/2, undoSize, undoSize)));

    // Draw Submit button (without RETURN) - top-right corner
    int submitX = clientSize.GetWidth() - 80;
    int submitY = 80;
    int submitSize = 100;

    gc->SetPen(wxPen(color, 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(submitX - submitSize/2, submitY - submitSize/2, submitSize, submitSize);

    gc->SetFont(undoFont, color);
    gc->GetTextExtent(wxT("Submit"), &textWidth, &textHeight);
    gc->DrawText(wxT("Submit"), submitX - textWidth/2, submitY - textHeight/2);

    // Store Submit button as a special key
    m_keyboardKeys.push_back(KeyboardKey(wxT("SUBMIT"), wxRect(submitX - submitSize/2, submitY - submitSize/2, submitSize, submitSize)));

    // Draw Submit w/ RETURN button - below the first submit button
    int submitReturnX = clientSize.GetWidth() - 80;
    int submitReturnY = 220;
    int submitReturnSize = 100;

    gc->SetPen(wxPen(color, 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(submitReturnX - submitReturnSize/2, submitReturnY - submitReturnSize/2, submitReturnSize, submitReturnSize);

    wxFont smallFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    gc->SetFont(smallFont, color);
    gc->GetTextExtent(wxT("Submit"), &textWidth, &textHeight);
    gc->DrawText(wxT("Submit"), submitReturnX - textWidth/2, submitReturnY - textHeight/2 - 10);
    gc->GetTextExtent(wxT("w/ Return"), &textWidth, &textHeight);
    gc->DrawText(wxT("w/ Return"), submitReturnX - textWidth/2, submitReturnY - textHeight/2 + 10);

    // Store Submit w/ RETURN button as a special key
    m_keyboardKeys.push_back(KeyboardKey(wxT("SUBMIT_RETURN"), wxRect(submitReturnX - submitReturnSize/2, submitReturnY - submitReturnSize/2, submitReturnSize, submitReturnSize)));

    // Restore previous progress for Undo button if it exists
    auto undoIter = std::find_if(m_keyboardKeys.begin(), m_keyboardKeys.end(),
                                  [](const KeyboardKey& k) { return k.label == wxT("UNDO"); });
    if (undoIter != m_keyboardKeys.end() && progressMap.count(wxT("UNDO")) > 0) {
        undoIter->dwellProgress = progressMap[wxT("UNDO")];
    }

    // Restore previous progress for Submit button if it exists
    auto submitIter = std::find_if(m_keyboardKeys.begin(), m_keyboardKeys.end(),
                                    [](const KeyboardKey& k) { return k.label == wxT("SUBMIT"); });
    if (submitIter != m_keyboardKeys.end() && progressMap.count(wxT("SUBMIT")) > 0) {
        submitIter->dwellProgress = progressMap[wxT("SUBMIT")];
    }

    // Restore previous progress for Submit w/ RETURN button if it exists
    if (progressMap.count(wxT("SUBMIT_RETURN")) > 0) {
        m_keyboardKeys.back().dwellProgress = progressMap[wxT("SUBMIT_RETURN")];
    }

    // Draw progress arc for Undo button if needed
    if (undoIter != m_keyboardKeys.end() && undoIter->dwellProgress > 0.0f) {
        gc->SetPen(wxPen(color, 6));
        wxGraphicsPath path = gc->CreatePath();
        path.AddArc(undoX, undoY, undoSize/2 - 5, 0, undoIter->dwellProgress * 2.0 * M_PI, true);
        gc->StrokePath(path);
    }

    // Draw progress arc for Submit button if needed
    if (submitIter != m_keyboardKeys.end() && submitIter->dwellProgress > 0.0f) {
        gc->SetPen(wxPen(color, 6));
        wxGraphicsPath path = gc->CreatePath();
        path.AddArc(submitX, submitY, submitSize/2 - 5, 0, submitIter->dwellProgress * 2.0 * M_PI, true);
        gc->StrokePath(path);
    }

    // Draw progress arc for Submit w/ RETURN button if needed
    float submitReturnProgress = m_keyboardKeys.back().dwellProgress;
    if (submitReturnProgress > 0.0f) {
        gc->SetPen(wxPen(color, 6));
        wxGraphicsPath path = gc->CreatePath();
        path.AddArc(submitReturnX, submitReturnY, submitReturnSize/2 - 5, 0, submitReturnProgress * 2.0 * M_PI, true);
        gc->StrokePath(path);
    }
}

void EyeOverlay::HandleKeyActivation(const wxString& keyLabel)
{
    wxLogMessage("Key activated: %s", keyLabel);

    if (keyLabel == wxT("UNDO")) {
        // Undo button - hide keyboard and return to normal overlay
        ShowKeyboard(false);
        m_keyboardKeys.clear();
    } else if (keyLabel == wxT("SUBMIT")) {
        // Submit button - send text to focused application (without RETURN)
        SubmitText();
    } else if (keyLabel == wxT("SUBMIT_RETURN")) {
        // Submit w/ RETURN button - send text and press RETURN
        SubmitTextWithReturn();
    } else if (keyLabel == wxT("SPACE")) {
        // Space key
        if (m_textEngine) {
            m_textEngine->AppendCharacter(wxT(' '));
        }
    } else if (keyLabel == wxT("⌫")) {
        // Backspace key
        if (m_textEngine) {
            m_textEngine->DeleteLastWord();
        }
    } else if (keyLabel.length() == 1) {
        // Regular letter/punctuation key
        if (m_textEngine) {
            // Convert back to lowercase for letters (display is uppercase, but input is lowercase)
            wxChar c = keyLabel[0];
            if (isupper(c)) {
                c = tolower(c);
            }
            m_textEngine->AppendCharacter(c);
        }
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

    // Handle keyboard key tracking if visible
    if (m_keyboardVisible) {
        bool needsRefresh = false;
        bool onKey = false;

        // Find which key (if any) is being hovered
        for (auto& key : m_keyboardKeys) {
            if (key.bounds.Contains(static_cast<int>(x), static_cast<int>(y))) {
                // Update progress
                float oldProgress = key.dwellProgress;
                key.dwellProgress += deltaTime / (m_settingHoldTime * 1000.0f);

                if (key.dwellProgress >= 1.0f) {
                    // Key activated!
                    key.dwellProgress = 0.0f;
                    HandleKeyActivation(key.label);
                    needsRefresh = true;
                } else if (static_cast<int>(key.dwellProgress * 20) != static_cast<int>(oldProgress * 20)) {
                    needsRefresh = true;
                }
                onKey = true;
            } else {
                // Reset progress if not hovering
                if (key.dwellProgress > 0.0f) {
                    key.dwellProgress = 0.0f;
                    needsRefresh = true;
                }
            }
        }

        // Always refresh when gaze cursor moves significantly (>5 pixels)
        float dx = m_gazePosition.m_x - oldPosition.m_x;
        float dy = m_gazePosition.m_y - oldPosition.m_y;
        float distanceMoved = std::sqrt(dx * dx + dy * dy);
        if (distanceMoved > 5.0f) {
            needsRefresh = true;
        }

        if (needsRefresh) {
            Refresh(false);
        }
        return;  // Don't process button logic when keyboard is visible
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
    }

    // Always refresh when gaze cursor moves significantly (>5 pixels to reduce 120Hz to ~20-30Hz)
    // This ensures the cursor is always visible and tracks the gaze position
    float dx = m_gazePosition.m_x - oldPosition.m_x;
    float dy = m_gazePosition.m_y - oldPosition.m_y;
    float distanceMoved = std::sqrt(dx * dx + dy * dy);
    if (distanceMoved > 5.0f) {
        needsRefresh = true;
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
        Hide();
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
        Show();  // Must call Show() since we called Hide() before screenshot
        Refresh();  // Force repaint to show buttons
    }

    // Create buttons at center of screen
    wxSize clientSize = GetClientSize();
    int centerX = clientSize.GetWidth() / 2;
    int centerY = clientSize.GetHeight() / 2;

    // Create buttons in radial pattern (matching HeyEyeControl layout)
    // All button positions are relative to window center

    // Keyboard button (top-left) - not shown in drag mode
    if (!m_isDragMode) {
        auto btnKeyboard = std::make_unique<CircularButton>(
            m_keyboardVisible ? wxT("Hide\nKeyboard") : wxT("Show\nKeyboard"),
            wxPoint(centerX - 175, centerY - 175)
        );
        btnKeyboard->OnActivated = [this]() {
            ShowKeyboard(!m_keyboardVisible);
            ClearAllButtons();
        };
        m_visibleButtons.push_back(std::move(btnKeyboard));
    }

    // Undo button (left) - in drag mode, releases the drag
    auto btnUndo = std::make_unique<CircularButton>(wxT("Undo"), wxPoint(centerX - 250, centerY));
    btnUndo->OnActivated = [this]() {
        if (m_isDragMode) {
            // In drag mode, Undo should release the drag
            Drop();
        } else {
            // In normal mode, just clear buttons
            ClearAllButtons();
        }
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
    wxMilliSleep(100);

    // Move cursor and click
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(m_settingCursorDelay);

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
    wxMilliSleep(100);

    // Move cursor and click
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(m_settingCursorDelay);

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
    wxMilliSleep(100);
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
    wxMilliSleep(100);

    // Move cursor and double click
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(m_settingCursorDelay);

    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    // Send twice for double click
    SendInput(2, inputs, sizeof(INPUT));
    wxMilliSleep(100);
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
    wxMilliSleep(100);

    // Move cursor and start drag (button DOWN only)
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(m_settingCursorDelay);

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
    wxMilliSleep(100);

    // Move cursor and release drag (button UP)
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(m_settingCursorDelay);

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

void EyeOverlay::SubmitText()
{
    if (!m_textEngine) return;

    wxString text = m_textEngine->GetCurrentText();
    if (text.IsEmpty()) {
        wxLogMessage("SubmitText: No text to submit");
        return;
    }

    wxLogMessage("SubmitText: Sending '%s' (without RETURN)", text);

    // Hide overlay
    m_visible = false;
    Hide();
    Refresh();
    Update();

#ifdef __WXMSW__
    wxMilliSleep(100);

    // Move cursor and click first (to focus the text field)
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(m_settingCursorDelay);

    // Perform click to focus
    INPUT clickInputs[2];
    ZeroMemory(clickInputs, sizeof(clickInputs));
    clickInputs[0].type = INPUT_MOUSE;
    clickInputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    clickInputs[1].type = INPUT_MOUSE;
    clickInputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, clickInputs, sizeof(INPUT));

    wxMilliSleep(100);  // Wait for focus to be set

    // Send each character as keyboard input
    for (size_t i = 0; i < text.length(); ++i) {
        wchar_t ch = text[i];

        INPUT inputs[2];
        ZeroMemory(inputs, sizeof(inputs));

        // Key down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = 0;
        inputs[0].ki.wScan = ch;
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[0].ki.time = 0;

        // Key up
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 0;
        inputs[1].ki.wScan = ch;
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs[1].ki.time = 0;

        SendInput(2, inputs, sizeof(INPUT));
    }

    wxLogMessage("SubmitText: Sent %zu characters (without RETURN)", text.length());
#endif

    // Clear the text after sending
    if (m_textEngine) {
        m_textEngine->Clear();
    }

    // Hide the keyboard after submit to prevent it from reappearing
    ShowKeyboard(false);
    m_keyboardKeys.clear();

    m_visible = true;
    Show();
    Refresh();
}

void EyeOverlay::SubmitTextWithReturn()
{
    if (!m_textEngine) return;

    wxString text = m_textEngine->GetCurrentText();
    if (text.IsEmpty()) {
        wxLogMessage("SubmitTextWithReturn: No text to submit");
        return;
    }

    wxLogMessage("SubmitTextWithReturn: Sending '%s' (with RETURN)", text);

    // Hide overlay
    m_visible = false;
    Hide();
    Refresh();
    Update();

#ifdef __WXMSW__
    wxMilliSleep(100);

    // Move cursor and click first (to focus the text field)
    SetCursorPos(m_screenshotPosition.x, m_screenshotPosition.y);

    wxMilliSleep(m_settingCursorDelay);

    // Perform click to focus
    INPUT clickInputs[2];
    ZeroMemory(clickInputs, sizeof(clickInputs));
    clickInputs[0].type = INPUT_MOUSE;
    clickInputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    clickInputs[1].type = INPUT_MOUSE;
    clickInputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, clickInputs, sizeof(INPUT));

    wxMilliSleep(100);  // Wait for focus to be set

    // Send each character as keyboard input
    for (size_t i = 0; i < text.length(); ++i) {
        wchar_t ch = text[i];

        INPUT inputs[2];
        ZeroMemory(inputs, sizeof(inputs));

        // Key down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = 0;
        inputs[0].ki.wScan = ch;
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[0].ki.time = 0;

        // Key up
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 0;
        inputs[1].ki.wScan = ch;
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs[1].ki.time = 0;

        SendInput(2, inputs, sizeof(INPUT));
    }

    wxLogMessage("SubmitTextWithReturn: Sent %zu characters", text.length());

    // Send RETURN key to validate the query
    wxMilliSleep(50);

    INPUT returnInputs[2];
    ZeroMemory(returnInputs, sizeof(returnInputs));

    // RETURN key down
    returnInputs[0].type = INPUT_KEYBOARD;
    returnInputs[0].ki.wVk = VK_RETURN;
    returnInputs[0].ki.dwFlags = 0;

    // RETURN key up
    returnInputs[1].type = INPUT_KEYBOARD;
    returnInputs[1].ki.wVk = VK_RETURN;
    returnInputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, returnInputs, sizeof(INPUT));
    wxLogMessage("SubmitTextWithReturn: Sent RETURN key");
#endif

    // Clear the text after sending
    if (m_textEngine) {
        m_textEngine->Clear();
    }

    // Hide the keyboard after submit to prevent it from reappearing
    ShowKeyboard(false);
    m_keyboardKeys.clear();

    m_visible = true;
    Show();
    Refresh();
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

bool EyeOverlay::IsTextCursorAtPosition(int x, int y)
{
#ifdef __WXMSW__
    // Get the current cursor information
    CURSORINFO ci;
    ci.cbSize = sizeof(CURSORINFO);

    if (GetCursorInfo(&ci)) {
        // Load the standard I-beam cursor (text edit cursor)
        HCURSOR hIBeam = LoadCursor(NULL, IDC_IBEAM);

        // Check if the current cursor matches the I-beam cursor
        if (ci.hCursor == hIBeam) {
            return true;
        }
    }
#endif
    return false;
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

                    // Recalculate screenshot source rect to center on new position
                    // Try to center the crosshair, but clamp to screen edges
                    int intendedX = m_screenshotPosition.x - m_settingSelectionWidth / 2;
                    int intendedY = m_screenshotPosition.y - m_settingSelectionHeight / 2;

                    m_screenshotSourceRect = wxRect(
                        intendedX,
                        intendedY,
                        m_settingSelectionWidth,
                        m_settingSelectionHeight
                    );

                    // Clamp to screen bounds
                    if (m_screenshotSourceRect.x < 0)
                        m_screenshotSourceRect.x = 0;
                    if (m_screenshotSourceRect.y < 0)
                        m_screenshotSourceRect.y = 0;
                    if (m_screenshotSourceRect.x + m_screenshotSourceRect.width > m_screenshot.GetWidth())
                        m_screenshotSourceRect.x = m_screenshot.GetWidth() - m_screenshotSourceRect.width;
                    if (m_screenshotSourceRect.y + m_screenshotSourceRect.height > m_screenshot.GetHeight())
                        m_screenshotSourceRect.y = m_screenshot.GetHeight() - m_screenshotSourceRect.height;

                    wxLogMessage("Zoom refinement: updated sourceRect to (%d, %d, %d, %d)",
                                m_screenshotSourceRect.x, m_screenshotSourceRect.y,
                                m_screenshotSourceRect.width, m_screenshotSourceRect.height);

                    // Exit zoom mode and recreate buttons
                    m_isZoomed = false;
                    m_dwellPosition = m_gazePosition;

                    // Check if we're on a text cursor - if so, show keyboard instead of buttons
                    if (m_settings && m_settings->GetAutoShowKeyboard() &&
                        IsTextCursorAtPosition(static_cast<int>(x), static_cast<int>(y))) {
                        wxLogMessage("Text cursor detected - showing keyboard instead of buttons");
                        // Save the dwell position for Submit to use
                        m_screenshotPosition = wxPoint(static_cast<int>(x), static_cast<int>(y));
                        ShowKeyboard(true);
                    } else {
                        CreateButtonsAtCenter();
                    }
                } else {
                    // Normal mode - create buttons at center
                    wxLogMessage("DWELL COMPLETE! Creating buttons at center...");
                    m_dwellPosition = m_gazePosition;

                    // Check if we're on a text cursor - if so, show keyboard instead of buttons
                    if (m_settings && m_settings->GetAutoShowKeyboard() &&
                        IsTextCursorAtPosition(static_cast<int>(x), static_cast<int>(y))) {
                        wxLogMessage("Text cursor detected - showing keyboard instead of buttons");
                        // Save the dwell position for Submit to use
                        m_screenshotPosition = wxPoint(static_cast<int>(x), static_cast<int>(y));
                        ShowKeyboard(true);
                    } else {
                        CreateButtonsAtCenter();
                    }
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
