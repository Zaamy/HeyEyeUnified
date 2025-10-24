#ifndef KEYBUTTON_H
#define KEYBUTTON_H

#include <wx/wx.h>
#include <wx/geometry.h>
#include <functional>

/**
 * @brief Represents a single key on the virtual keyboard
 *
 * Handles both dwell-based selection (letter-by-letter mode) and
 * swipe path tracking (ML mode). Features:
 * - Visual feedback with progress arc for dwell time
 * - Hover state tracking
 * - Position and character mapping
 */
class KeyButton
{
public:
    explicit KeyButton(wxChar character, const wxRect2DDouble& geometry);

    // Getters
    wxChar GetCharacter() const { return m_character; }
    wxRect2DDouble GetGeometry() const { return m_geometry; }
    bool IsHovered() const { return m_hovered; }
    float GetProgress() const { return m_progress; }
    bool IsHighlighted() const { return m_highlighted; }

    // Setters
    void SetGeometry(const wxRect2DDouble& rect) { m_geometry = rect; }
    void SetHovered(bool hovered);
    void SetProgress(float progress); // 0.0 to 1.0
    void SetHighlighted(bool highlighted) { m_highlighted = highlighted; }

    // Drawing
    void Draw(wxDC& dc, const wxColour& normalColor, const wxColour& hoverColor, const wxColour& progressColor);

    // Hit testing
    bool Contains(const wxPoint2DDouble& point) const;

    // Callback for activation (replaces Qt signal)
    std::function<void()> OnActivated;

private:
    wxChar m_character;
    wxRect2DDouble m_geometry;
    bool m_hovered;
    float m_progress; // 0.0 to 1.0 for dwell-time progress
    bool m_highlighted; // For swipe path highlighting
};

#endif // KEYBUTTON_H
