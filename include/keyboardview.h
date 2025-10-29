#ifndef KEYBOARDVIEW_H
#define KEYBOARDVIEW_H

#include <wx/wx.h>
#include <wx/geometry.h>
#include <vector>
#include <map>
#include <utility>
#include <functional>
#include "keybutton.h"

/**
 * @brief Input modes for the keyboard
 */
enum class InputMode {
    LetterByLetter,  // Dwell-based selection of individual letters
    Swipe            // ML-based swipe gesture recognition
};

/**
 * @brief Key rendering information for manual rendering on overlay
 */
struct KeyRenderInfo {
    wxRect2DDouble geometry;      // Key position and size
    wxString label;               // Display label
    float progress;               // Dwell progress (0.0 to 1.0)
    bool isHovered;               // Currently hovered by gaze
    bool isHighlighted;           // Highlighted (for swipe visualization)
    bool isModifierActive;        // For modifier keys (shift/caps/altgr)
    KeyType keyType;              // Type of key

    KeyRenderInfo()
        : geometry()
        , label()
        , progress(0.0f)
        , isHovered(false)
        , isHighlighted(false)
        , isModifierActive(false)
        , keyType(KeyType::Character)
    {}
};

/**
 * @brief Visual keyboard widget managing all keys and input modes
 *
 * Features:
 * - AZERTY French keyboard layout
 * - Dual input modes: letter-by-letter dwell and swipe ML
 * - Gaze position tracking and visualization
 * - Swipe path recording and rendering
 */
class KeyboardView : public wxPanel
{
public:
    explicit KeyboardView(wxWindow *parent);
    ~KeyboardView();

    // Input mode management
    InputMode GetInputMode() const { return m_inputMode; }
    void SetInputMode(InputMode mode);

    // Gaze tracking
    void UpdateGazePosition(float x, float y);

    // Swipe recording
    void StartSwipeRecording();
    void StopSwipeRecording();
    bool IsRecordingSwipe() const { return m_recordingSwipe; }
    const std::vector<std::pair<float, float>>& GetSwipePath() const { return m_swipePath; }
    void ClearSwipePath();

    // Key highlighting (for ML result visualization)
    void HighlightKey(wxChar character, bool highlight = true);
    void ClearHighlights();

    // Settings
    void SetDwellTime(int milliseconds) { m_dwellTimeMs = milliseconds; }
    int GetDwellTime() const { return m_dwellTimeMs; }

    // Modifier state
    bool IsShiftActive() const { return m_shiftActive; }
    bool IsCapsLockActive() const { return m_capsLockActive; }
    bool IsAltGrActive() const { return m_altgrActive; }

    // Get keyboard coordinates for DTW computation
    std::map<wxChar, std::pair<float, float>> GetKeyboardCoordinates() const;

    // Render keyboard to a DC (for manual rendering on overlay)
    void RenderToDC(wxDC& dc);

    // Get all keys for manual rendering with wxGraphicsContext
    std::vector<KeyRenderInfo> GetKeysForRendering() const;

    // Callbacks (replace Qt signals)
    std::function<void(wxChar)> OnLetterSelected;
    std::function<void(const std::vector<std::pair<float, float>>&)> OnSwipeCompleted;
    std::function<void()> OnSpacePressed;
    std::function<void()> OnBackspacePressed;
    std::function<void()> OnEnterPressed;

protected:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

private:
    void CreateKeyboard();
    void UpdateKeyGeometries();
    void UpdateDwellProgress(KeyButton *key, float deltaMs);
    KeyButton* FindKeyAtPosition(const wxPoint2DDouble &pos);
    void ToggleShift();
    void ToggleCapsLock();
    void ToggleAltGr();
    wxChar GetEffectiveCharacter(KeyButton* key);

    // Input mode
    InputMode m_inputMode;

    // Keyboard layout
    std::vector<KeyButton*> m_keys;
    KeyButton* m_spaceKey;
    KeyButton* m_shiftKey;
    KeyButton* m_capsLockKey;
    KeyButton* m_altgrKey;
    KeyButton* m_backspaceKey;
    KeyButton* m_enterKey;
    std::map<wxChar, KeyButton*> m_keyMap;

    // Modifier states
    bool m_shiftActive;
    bool m_capsLockActive;
    bool m_altgrActive;

    // Gaze tracking
    wxPoint2DDouble m_gazePosition;
    KeyButton* m_currentHoveredKey;
    wxLongLong m_lastUpdateTime;

    // Dwell-time settings
    int m_dwellTimeMs;

    // Swipe recording
    bool m_recordingSwipe;
    std::vector<std::pair<float, float>> m_swipePath;

    // Visual settings
    wxColour m_normalColor;
    wxColour m_hoverColor;
    wxColour m_progressColor;
    wxColour m_swipePathColor;
    float m_keySpacing;

    wxDECLARE_EVENT_TABLE();
};

#endif // KEYBOARDVIEW_H
