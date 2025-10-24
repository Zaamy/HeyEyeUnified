#ifndef CIRCULARBUTTON_H
#define CIRCULARBUTTON_H

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <functional>
#include <string>

/**
 * @brief Circular button with dwell-time progress arc (mimics HeyEyeControl EyeButton)
 *
 * Features:
 * - Circular shape with customizable size (default 120x120 like HeyEyeControl)
 * - Text label displayed in center
 * - Progress arc that fills as user dwells on button
 * - Activates when progress reaches 100%
 * - No images (text-only fallback from EyeButton)
 */
class CircularButton
{
public:
    CircularButton(const wxString& label, const wxPoint& position);
    ~CircularButton() = default;

    // Rendering
    void Draw(wxDC& dc, const wxColour& color);

    // Interaction
    bool IsPointInside(float x, float y) const;
    bool UpdateProgress(float deltaTime, float holdTime); // Returns true if visual state changed (deltaTime in microseconds)
    bool ResetProgress();  // Returns true if progress was reset (visual state changed)
    bool IsActivated() const;

    // Callback when button is activated
    std::function<void()> OnActivated;

    // Properties
    wxString GetLabel() const { return m_label; }
    void SetLabel(const wxString& label) { m_label = label; }

    wxPoint GetPosition() const { return m_position; }
    void SetPosition(const wxPoint& pos) { m_position = pos; }

    wxSize GetSize() const { return m_size; }
    void SetSize(const wxSize& size) { m_size = size; }

    float GetProgress() const { return m_progress; }

    bool IsSelected() const { return m_isSelected; }
    void SetSelected(bool selected) { m_isSelected = selected; }

private:
    wxString m_label;
    wxPoint m_position;  // Center of the circle
    wxSize m_size;       // Default 120x120 like EyeButton
    float m_progress;    // 0.0 to 1.0
    bool m_isSelected;   // For special highlighting (e.g., scroll mode)
};

#endif // CIRCULARBUTTON_H
