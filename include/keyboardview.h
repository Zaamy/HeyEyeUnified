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
 * @brief Key rendering information for manual rendering on overlay
 */
struct KeyRenderInfo {
    wxRect2DDouble geometry;      // Key position and size
    wxString primaryLabel;        // Primary character (center/bottom)
    wxString shiftLabel;          // Shift character (top-left)
    wxString altgrLabel;          // AltGr character (top-right)
    float progress;               // Dwell progress (0.0 to 1.0)
    bool isHovered;               // Currently hovered by gaze
    bool isModifierActive;        // For modifier keys (shift/caps/altgr)
    KeyType keyType;              // Type of key

    // Which character layer is currently active (will be typed)
    enum ActiveLayer {
        Primary,
        Shift,
        AltGr
    };
    ActiveLayer activeLayer;      // Which character is active based on modifiers

    KeyRenderInfo()
        : geometry()
        , primaryLabel()
        , shiftLabel()
        , altgrLabel()
        , progress(0.0f)
        , isHovered(false)
        , isModifierActive(false)
        , keyType(KeyType::Character)
        , activeLayer(Primary)
    {}
};

/**
 * @brief Visual keyboard widget managing all keys and input modes
 *
 * Features:
 * - AZERTY French keyboard layout
 * - Letter-by-letter dwell (always active)
 * - Optional swipe ML (can be enabled/disabled)
 * - Gaze position tracking and visualization
 * - Swipe path recording and rendering
 */
class KeyboardView : public wxPanel
{
public:
    explicit KeyboardView(wxWindow *parent);
    ~KeyboardView();

    // Swipe mode management (LetterByLetter is always active)
    bool IsSwipeEnabled() const { return m_swipeEnabled; }
    void SetSwipeEnabled(bool enabled);

    // Gaze tracking
    void UpdateGazePosition(float x, float y);

    // Swipe recording (automatically managed by vertical position)
    bool IsRecordingSwipe() const { return m_recordingSwipe; }
    const std::vector<std::pair<float, float>>& GetSwipePath() const { return m_swipePath; }
    void ClearSwipePath();

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
    std::function<void()> OnDeleteWordPressed;
    std::function<void()> OnEnterPressed;
    std::function<void()> OnSpeakPressed;

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

    // Swipe detection helpers
    void StartSwipeRecording();
    void StopSwipeRecording();

    // Swipe mode (LetterByLetter is always active)
    bool m_swipeEnabled;

    // Keyboard layout
    std::vector<KeyButton*> m_keys;
    KeyButton* m_spaceKey;
    KeyButton* m_shiftKey;
    KeyButton* m_capsLockKey;
    KeyButton* m_altgrKey;
    KeyButton* m_backspaceKey;
    KeyButton* m_deleteWordKey;
    KeyButton* m_enterKey;
    KeyButton* m_swipeToggleKey;
    KeyButton* m_speakKey;
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
    wxPoint2DDouble m_previousGazePosition;  // Track previous position for exit detection

    // Visual settings
    wxColour m_normalColor;
    wxColour m_hoverColor;
    wxColour m_progressColor;
    wxColour m_swipePathColor;
    float m_keySpacing;
    float m_keySize;  // Current key size for coordinate normalization

    wxDECLARE_EVENT_TABLE();
};

#endif // KEYBOARDVIEW_H
