#ifndef KEYBUTTON_H
#define KEYBUTTON_H

#include <wx/wx.h>
#include <wx/geometry.h>
#include <functional>

/**
 * @brief Type of key (regular character or modifier)
 */
enum class KeyType {
    Character,     // Regular character key
    Shift,         // Shift modifier
    CapsLock,      // Caps Lock modifier
    AltGr,         // AltGr modifier
    Backspace,     // Backspace key
    Enter          // Enter key
};

/**
 * @brief Represents a single key on the virtual keyboard
 *
 * Handles both dwell-based selection (letter-by-letter mode) and
 * swipe path tracking (ML mode). Features:
 * - Visual feedback with progress arc for dwell time
 * - Hover state tracking
 * - Position and character mapping
 * - Support for shift, caps lock, and AltGr modifiers
 */
class KeyButton
{
public:
    // Constructor for character keys with shift/altgr variants
    explicit KeyButton(wxChar primary, wxChar shift, wxChar altgr, const wxRect2DDouble& geometry);

    // Constructor for modifier/special keys
    explicit KeyButton(KeyType type, const wxString& label, const wxRect2DDouble& geometry);

    // Getters
    KeyType GetKeyType() const { return m_keyType; }
    wxChar GetPrimaryCharacter() const { return m_primaryChar; }
    wxChar GetShiftCharacter() const { return m_shiftChar; }
    wxChar GetAltGrCharacter() const { return m_altgrChar; }
    wxString GetLabel() const { return m_label; }
    wxRect2DDouble GetGeometry() const { return m_geometry; }
    bool IsHovered() const { return m_hovered; }
    float GetProgress() const { return m_progress; }
    bool IsHighlighted() const { return m_highlighted; }
    bool IsModifierActive() const { return m_modifierActive; }

    // Setters
    void SetGeometry(const wxRect2DDouble& rect) { m_geometry = rect; }
    void SetHovered(bool hovered);
    void SetProgress(float progress); // 0.0 to 1.0
    void SetHighlighted(bool highlighted) { m_highlighted = highlighted; }
    void SetModifierActive(bool active) { m_modifierActive = active; }

    // Drawing
    void Draw(wxDC& dc, const wxColour& normalColor, const wxColour& hoverColor, const wxColour& progressColor,
              bool shiftActive, bool capsActive, bool altgrActive);

    // Hit testing
    bool Contains(const wxPoint2DDouble& point) const;

    // Callback for activation (replaces Qt signal)
    std::function<void()> OnActivated;

private:
    KeyType m_keyType;
    wxChar m_primaryChar;
    wxChar m_shiftChar;
    wxChar m_altgrChar;
    wxString m_label;  // For modifier keys
    wxRect2DDouble m_geometry;
    bool m_hovered;
    float m_progress; // 0.0 to 1.0 for dwell-time progress
    bool m_highlighted; // For swipe path highlighting
    bool m_modifierActive; // For visual feedback on modifier keys
};

#endif // KEYBUTTON_H
