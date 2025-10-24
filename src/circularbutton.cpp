#include "CircularButton.h"
#include <cmath>

CircularButton::CircularButton(const wxString& label, const wxPoint& position)
    : m_label(label)
    , m_position(position)
    , m_size(120, 120)  // Default size from EyeButton
    , m_progress(0.0f)
    , m_isSelected(false)
{
}

void CircularButton::Draw(wxDC& dc, const wxColour& color)
{
    // Draw text label (fallback rendering from EyeButton when image is null)
    dc.SetTextForeground(color);
    dc.SetTextBackground(wxColour(0, 0, 0, 0));  // Transparent text background
    dc.SetBackgroundMode(wxTRANSPARENT);  // CRITICAL: Prevents black rectangles behind text
    dc.SetFont(wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));

    wxSize textSize = dc.GetTextExtent(m_label);
    wxPoint textPos(
        m_position.x - textSize.GetWidth() / 2,
        m_position.y - textSize.GetHeight() / 2
    );
    dc.DrawText(m_label, textPos);

    // Draw circle around the button
    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    int penWidth = m_isSelected ? 3 : 1;
    dc.SetPen(wxPen(color, penWidth));
    dc.DrawCircle(m_position, m_size.GetWidth() / 2);

    // Draw progress arc (like EyeButton line 34-37)
    if (m_progress > 0.0f) {
        dc.SetPen(wxPen(color, 6));

        int reduce = 4;  // Arc is slightly inside the circle
        int arcX = m_position.x - m_size.GetWidth() / 2 + reduce;
        int arcY = m_position.y - m_size.GetHeight() / 2 + reduce;
        int arcWidth = m_size.GetWidth() - 2 * reduce;
        int arcHeight = m_size.GetHeight() - 2 * reduce;

        // Convert progress (0-1) to arc angle in degrees
        double arcAngle = m_progress * 360.0;

        // wxWidgets DrawEllipticArc: (x, y, width, height, start_angle, end_angle)
        // Arc starts at 0° (3 o'clock) and goes counter-clockwise
        dc.DrawEllipticArc(
            arcX,
            arcY,
            arcWidth,
            arcHeight,
            0.0,        // Start at 0° (3 o'clock)
            arcAngle    // End angle
        );
    }
}

bool CircularButton::IsPointInside(float x, float y) const
{
    float dx = x - m_position.x;
    float dy = y - m_position.y;
    float distSq = dx * dx + dy * dy;
    float radiusSq = (m_size.GetWidth() / 2.0f) * (m_size.GetHeight() / 2.0f);
    return distSq <= radiusSq;
}

bool CircularButton::UpdateProgress(float deltaTime, float holdTime)
{
    // deltaTime in microseconds, holdTime in milliseconds
    // Convert holdTime to microseconds
    float holdTimeUs = holdTime * 1000.0f;

    float oldProgress = m_progress;
    m_progress += deltaTime / holdTimeUs;

    if (m_progress >= 1.0f) {
        m_progress = 0.0f;
        if (OnActivated) {
            OnActivated();
        }
        return true;  // Button activated - visual state changed
    }

    // Only trigger refresh if progress changed by at least 5% (reduces refresh rate)
    return (static_cast<int>(m_progress * 20) != static_cast<int>(oldProgress * 20));
}

bool CircularButton::ResetProgress()
{
    if (m_progress > 0.0f) {
        m_progress = 0.0f;
        return true;  // Visual state changed (progress was reset)
    }
    return false;  // Progress was already 0
}

bool CircularButton::IsActivated() const
{
    return m_progress >= 1.0f;
}
