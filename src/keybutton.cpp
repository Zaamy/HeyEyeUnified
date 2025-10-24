#include "keybutton.h"
#include <wx/graphics.h>
#include <algorithm>

KeyButton::KeyButton(wxChar character, const wxRect2DDouble& geometry)
    : m_character(character)
    , m_geometry(geometry)
    , m_hovered(false)
    , m_progress(0.0f)
    , m_highlighted(false)
    , OnActivated(nullptr)
{
}

void KeyButton::SetHovered(bool hovered)
{
    if (m_hovered != hovered) {
        m_hovered = hovered;
        if (!hovered) {
            m_progress = 0.0f; // Reset progress when hover ends
        }
    }
}

void KeyButton::SetProgress(float progress)
{
    m_progress = std::max(0.0f, std::min(1.0f, progress));

    // Call activation callback when progress reaches 100%
    if (m_progress >= 1.0f && OnActivated) {
        OnActivated();
    }
}

void KeyButton::Draw(wxDC& dc, const wxColour& normalColor, const wxColour& hoverColor, const wxColour& progressColor)
{
    // Determine background color based on state
    wxColour bgColor = normalColor;
    if (m_hovered || m_highlighted) {
        bgColor = hoverColor;
    }

    // Draw key background
    dc.SetBrush(wxBrush(bgColor));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(
        static_cast<int>(m_geometry.m_x),
        static_cast<int>(m_geometry.m_y),
        static_cast<int>(m_geometry.m_width),
        static_cast<int>(m_geometry.m_height)
    );

    // Draw border
    dc.SetPen(wxPen(*wxLIGHT_GREY, 2));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(
        static_cast<int>(m_geometry.m_x),
        static_cast<int>(m_geometry.m_y),
        static_cast<int>(m_geometry.m_width),
        static_cast<int>(m_geometry.m_height)
    );

    // Draw character
    wxFont font(static_cast<int>(m_geometry.m_height * 0.4), wxFONTFAMILY_DEFAULT,
                wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxT("Segoe UI"));
    dc.SetFont(font);
    dc.SetTextForeground(*wxBLACK);

    wxString charStr;
    charStr << m_character;

    wxCoord textWidth, textHeight;
    dc.GetTextExtent(charStr, &textWidth, &textHeight);

    int textX = static_cast<int>(m_geometry.m_x + (m_geometry.m_width - textWidth) / 2);
    int textY = static_cast<int>(m_geometry.m_y + (m_geometry.m_height - textHeight) / 2);

    dc.DrawText(charStr, textX, textY);

    // Draw progress arc if there's any progress
    if (m_progress > 0.0f && m_progress < 1.0f) {
        dc.SetPen(wxPen(progressColor, 4));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        // Create inset rectangle for arc
        wxRect2DDouble arcRect = m_geometry;
        arcRect.Inset(4, 4);

        // wxDC::DrawEllipticArc uses degrees (0-360)
        // Start at top (270 degrees) and go clockwise
        double startAngle = 90.0;  // Top of circle
        double endAngle = 90.0 - (360.0 * m_progress);  // Clockwise

        dc.DrawEllipticArc(
            static_cast<int>(arcRect.m_x),
            static_cast<int>(arcRect.m_y),
            static_cast<int>(arcRect.m_width),
            static_cast<int>(arcRect.m_height),
            startAngle,
            endAngle
        );
    }

    // Draw full circle if progress is complete
    if (m_progress >= 1.0f) {
        dc.SetPen(wxPen(progressColor, 4));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        wxRect2DDouble arcRect = m_geometry;
        arcRect.Inset(4, 4);

        dc.DrawEllipse(
            static_cast<int>(arcRect.m_x),
            static_cast<int>(arcRect.m_y),
            static_cast<int>(arcRect.m_width),
            static_cast<int>(arcRect.m_height)
        );
    }
}

bool KeyButton::Contains(const wxPoint2DDouble& point) const
{
    return m_geometry.Contains(point);
}
