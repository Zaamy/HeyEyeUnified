#include "keybutton.h"
#include <wx/graphics.h>
#include <algorithm>

KeyButton::KeyButton(wxChar primary, wxChar shift, wxChar altgr, const wxRect2DDouble& geometry)
    : m_keyType(KeyType::Character)
    , m_primaryChar(primary)
    , m_shiftChar(shift)
    , m_altgrChar(altgr)
    , m_label(wxEmptyString)
    , m_geometry(geometry)
    , m_hovered(false)
    , m_progress(0.0f)
    , m_modifierActive(false)
    , OnActivated(nullptr)
{
}

KeyButton::KeyButton(KeyType type, const wxString& label, const wxRect2DDouble& geometry)
    : m_keyType(type)
    , m_primaryChar(0)
    , m_shiftChar(0)
    , m_altgrChar(0)
    , m_label(label)
    , m_geometry(geometry)
    , m_hovered(false)
    , m_progress(0.0f)
    , m_modifierActive(false)
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

void KeyButton::Draw(wxDC& dc, const wxColour& normalColor, const wxColour& hoverColor, const wxColour& progressColor,
                     bool shiftActive, bool capsActive, bool altgrActive)
{
    // Determine background color based on state
    wxColour bgColor = normalColor;
    if (m_hovered) {
        bgColor = hoverColor;
    }

    // Modifier keys use different color when active
    if (m_modifierActive && (m_keyType == KeyType::Shift || m_keyType == KeyType::CapsLock || m_keyType == KeyType::AltGr)) {
        bgColor = wxColour(255, 200, 100); // Orange for active modifiers
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

    // Draw content based on key type
    if (m_keyType == KeyType::Character) {
        // Draw characters (primary in center/bottom, shift/altgr in corners)
        wxFont primaryFont(static_cast<int>(m_geometry.m_height * 0.35), wxFONTFAMILY_DEFAULT,
                          wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxT("Segoe UI"));
        wxFont secondaryFont(static_cast<int>(m_geometry.m_height * 0.2), wxFONTFAMILY_DEFAULT,
                            wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxT("Segoe UI"));

        // Draw primary character (centered)
        dc.SetFont(primaryFont);
        dc.SetTextForeground(*wxBLACK);
        wxString primaryStr;
        primaryStr << m_primaryChar;

        wxCoord primaryWidth, primaryHeight;
        dc.GetTextExtent(primaryStr, &primaryWidth, &primaryHeight);

        int primaryX = static_cast<int>(m_geometry.m_x + (m_geometry.m_width - primaryWidth) / 2);
        int primaryY = static_cast<int>(m_geometry.m_y + (m_geometry.m_height - primaryHeight) / 2 + m_geometry.m_height * 0.1);

        dc.DrawText(primaryStr, primaryX, primaryY);

        // Draw shift character (top-left corner)
        if (m_shiftChar != 0 && m_shiftChar != m_primaryChar) {
            dc.SetFont(secondaryFont);
            dc.SetTextForeground(wxColour(100, 100, 100));
            wxString shiftStr;
            shiftStr << m_shiftChar;

            int shiftX = static_cast<int>(m_geometry.m_x + 4);
            int shiftY = static_cast<int>(m_geometry.m_y + 4);

            dc.DrawText(shiftStr, shiftX, shiftY);
        }

        // Draw AltGr character (top-right corner)
        if (m_altgrChar != 0 && m_altgrChar != m_primaryChar && m_altgrChar != m_shiftChar) {
            dc.SetFont(secondaryFont);
            dc.SetTextForeground(wxColour(100, 100, 100));
            wxString altgrStr;
            altgrStr << m_altgrChar;

            wxCoord altgrWidth, altgrHeight;
            dc.GetTextExtent(altgrStr, &altgrWidth, &altgrHeight);

            int altgrX = static_cast<int>(m_geometry.m_x + m_geometry.m_width - altgrWidth - 4);
            int altgrY = static_cast<int>(m_geometry.m_y + 4);

            dc.DrawText(altgrStr, altgrX, altgrY);
        }
    } else {
        // Draw label for modifier/special keys
        wxFont labelFont(static_cast<int>(m_geometry.m_height * 0.25), wxFONTFAMILY_DEFAULT,
                        wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false, wxT("Segoe UI"));
        dc.SetFont(labelFont);
        dc.SetTextForeground(*wxBLACK);

        wxCoord labelWidth, labelHeight;
        dc.GetTextExtent(m_label, &labelWidth, &labelHeight);

        int labelX = static_cast<int>(m_geometry.m_x + (m_geometry.m_width - labelWidth) / 2);
        int labelY = static_cast<int>(m_geometry.m_y + (m_geometry.m_height - labelHeight) / 2);

        dc.DrawText(m_label, labelX, labelY);
    }

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
