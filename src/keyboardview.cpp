#include "keyboardview.h"
#include <wx/dcbuffer.h>
#include <algorithm>
#include <cmath>

wxBEGIN_EVENT_TABLE(KeyboardView, wxPanel)
    EVT_PAINT(KeyboardView::OnPaint)
    EVT_SIZE(KeyboardView::OnSize)
wxEND_EVENT_TABLE()

KeyboardView::KeyboardView(wxWindow *parent)
    : wxPanel(parent, wxID_ANY)
    , m_inputMode(InputMode::LetterByLetter)
    , m_spaceKey(nullptr)
    , m_gazePosition(0, 0)
    , m_currentHoveredKey(nullptr)
    , m_lastUpdateTime(wxGetLocalTimeMillis())
    , m_dwellTimeMs(800)
    , m_recordingSwipe(false)
    , m_normalColor(240, 240, 240)
    , m_hoverColor(102, 204, 255)
    , m_progressColor(0, 150, 255)
    , m_swipePathColor(255, 100, 100)
    , m_keySpacing(4.0f)
    , OnLetterSelected(nullptr)
    , OnSwipeCompleted(nullptr)
    , OnSpacePressed(nullptr)
{
    // AZERTY French keyboard layout
    m_keyboardLayout.push_back(wxT("&é\"'(-è_çà)="));
    m_keyboardLayout.push_back(wxT("azertyuiop^$"));
    m_keyboardLayout.push_back(wxT("qsdfghjklmù*"));
    m_keyboardLayout.push_back(wxT("<wxcvbn,;:!"));

    CreateKeyboard();

    // Set background style for better rendering
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

KeyboardView::~KeyboardView()
{
    // Clean up keys
    for (KeyButton* key : m_keys) {
        delete key;
    }
    delete m_spaceKey;
}

void KeyboardView::SetInputMode(InputMode mode)
{
    if (m_inputMode != mode) {
        m_inputMode = mode;

        // Reset state when switching modes
        if (m_currentHoveredKey) {
            m_currentHoveredKey->SetHovered(false);
            m_currentHoveredKey->SetProgress(0.0f);
            m_currentHoveredKey = nullptr;
        }

        ClearSwipePath();
        ClearHighlights();

        Refresh();
    }
}

void KeyboardView::UpdateGazePosition(float x, float y)
{
    wxLongLong currentTime = wxGetLocalTimeMillis();
    float deltaMs = (currentTime - m_lastUpdateTime).ToDouble();
    m_lastUpdateTime = currentTime;

    // Convert to widget coordinates
    wxPoint screenPos(static_cast<int>(x), static_cast<int>(y));
    wxPoint localPos = ScreenToClient(screenPos);
    m_gazePosition = wxPoint2DDouble(localPos.x, localPos.y);

    // Find key at current position
    KeyButton* hoveredKey = FindKeyAtPosition(m_gazePosition);

    // Handle swipe recording
    if (m_recordingSwipe && m_inputMode == InputMode::Swipe) {
        // Record point relative to keyboard
        m_swipePath.push_back(std::make_pair(
            static_cast<float>(m_gazePosition.m_x),
            static_cast<float>(m_gazePosition.m_y)
        ));
    }

    // Handle dwell-time in letter-by-letter mode
    if (m_inputMode == InputMode::LetterByLetter) {
        if (hoveredKey != m_currentHoveredKey) {
            // Hover changed
            if (m_currentHoveredKey) {
                m_currentHoveredKey->SetHovered(false);
                m_currentHoveredKey->SetProgress(0.0f);
            }

            m_currentHoveredKey = hoveredKey;

            if (m_currentHoveredKey) {
                m_currentHoveredKey->SetHovered(true);
            }
        } else if (m_currentHoveredKey) {
            // Continue hovering on same key
            UpdateDwellProgress(m_currentHoveredKey, deltaMs);
        }
    }

    Refresh();
}

void KeyboardView::StartSwipeRecording()
{
    if (m_inputMode == InputMode::Swipe) {
        m_recordingSwipe = true;
        m_swipePath.clear();
    }
}

void KeyboardView::StopSwipeRecording()
{
    if (m_recordingSwipe) {
        m_recordingSwipe = false;

        if (!m_swipePath.empty() && OnSwipeCompleted) {
            OnSwipeCompleted(m_swipePath);
        }
    }
}

void KeyboardView::ClearSwipePath()
{
    m_swipePath.clear();
    Refresh();
}

void KeyboardView::HighlightKey(wxChar character, bool highlight)
{
    auto it = m_keyMap.find(character);
    if (it != m_keyMap.end()) {
        it->second->SetHighlighted(highlight);
        Refresh();
    }
}

void KeyboardView::ClearHighlights()
{
    for (KeyButton* key : m_keys) {
        key->SetHighlighted(false);
    }
    if (m_spaceKey) {
        m_spaceKey->SetHighlighted(false);
    }
    Refresh();
}

std::map<wxChar, std::pair<float, float>> KeyboardView::GetKeyboardCoordinates() const
{
    std::map<wxChar, std::pair<float, float>> coords;

    for (KeyButton* key : m_keys) {
        wxRect2DDouble rect = key->GetGeometry();
        float centerX = static_cast<float>(rect.m_x + rect.m_width / 2);
        float centerY = static_cast<float>(rect.m_y + rect.m_height / 2);
        coords[key->GetCharacter()] = std::make_pair(centerX, centerY);
    }

    if (m_spaceKey) {
        wxRect2DDouble rect = m_spaceKey->GetGeometry();
        float centerX = static_cast<float>(rect.m_x + rect.m_width / 2);
        float centerY = static_cast<float>(rect.m_y + rect.m_height / 2);
        coords[wxT(' ')] = std::make_pair(centerX, centerY);
    }

    return coords;
}

void KeyboardView::OnPaint(wxPaintEvent& event)
{
    wxUnusedVar(event);

    // Use buffered paint DC for smooth rendering
    wxBufferedPaintDC dc(this);

    // Clear background
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();

    // Draw all keys
    for (KeyButton* key : m_keys) {
        key->Draw(dc, m_normalColor, m_hoverColor, m_progressColor);
    }

    // Draw space bar
    if (m_spaceKey) {
        m_spaceKey->Draw(dc, m_normalColor, m_hoverColor, m_progressColor);
    }

    // Draw swipe path
    if (!m_swipePath.empty() && m_inputMode == InputMode::Swipe) {
        dc.SetPen(wxPen(m_swipePathColor, 3));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        // Draw lines between points
        for (size_t i = 1; i < m_swipePath.size(); ++i) {
            wxPoint p1(
                static_cast<int>(m_swipePath[i - 1].first),
                static_cast<int>(m_swipePath[i - 1].second)
            );
            wxPoint p2(
                static_cast<int>(m_swipePath[i].first),
                static_cast<int>(m_swipePath[i].second)
            );
            dc.DrawLine(p1, p2);
        }

        // Draw dots at each point
        dc.SetBrush(wxBrush(m_swipePathColor));
        for (const auto& point : m_swipePath) {
            dc.DrawCircle(
                static_cast<int>(point.first),
                static_cast<int>(point.second),
                2
            );
        }
    }

    // Draw gaze cursor
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(255, 0, 0, 150)));
    dc.DrawCircle(
        static_cast<int>(m_gazePosition.m_x),
        static_cast<int>(m_gazePosition.m_y),
        8
    );
}

void KeyboardView::OnSize(wxSizeEvent& event)
{
    event.Skip();
    UpdateKeyGeometries();
    Refresh();
}

void KeyboardView::CreateKeyboard()
{
    // Clear existing keys
    for (KeyButton* key : m_keys) {
        delete key;
    }
    m_keys.clear();
    m_keyMap.clear();

    // Create keys based on layout
    for (const wxString& line : m_keyboardLayout) {
        for (size_t i = 0; i < line.Length(); ++i) {
            wxChar c = line[i];
            KeyButton* key = new KeyButton(c, wxRect2DDouble());

            // Set up activation callback
            key->OnActivated = [this, c]() {
                if (m_inputMode == InputMode::LetterByLetter && OnLetterSelected) {
                    OnLetterSelected(c);
                    // Reset after selection
                    if (m_currentHoveredKey) {
                        m_currentHoveredKey->SetProgress(0.0f);
                    }
                }
            };

            m_keys.push_back(key);
            m_keyMap[c] = key;
        }
    }

    // Create space bar
    m_spaceKey = new KeyButton(wxT(' '), wxRect2DDouble());
    m_spaceKey->OnActivated = [this]() {
        if (m_inputMode == InputMode::LetterByLetter && OnSpacePressed) {
            OnSpacePressed();
            if (m_currentHoveredKey == m_spaceKey) {
                m_spaceKey->SetProgress(0.0f);
            }
        }
    };

    UpdateKeyGeometries();
}

void KeyboardView::UpdateKeyGeometries()
{
    wxSize clientSize = GetClientSize();
    if (clientSize.GetWidth() <= 0 || clientSize.GetHeight() <= 0) {
        return;
    }

    // Calculate key size based on widget dimensions
    float availableWidth = clientSize.GetWidth() - m_keySpacing * 14;
    float keyWidth = availableWidth / 13.0f;

    float availableHeight = clientSize.GetHeight() - m_keySpacing * 6;
    float keyHeight = availableHeight / 5.0f;

    float keySize = std::min(keyWidth, keyHeight);

    size_t keyIndex = 0;
    for (size_t row = 0; row < m_keyboardLayout.size(); ++row) {
        wxString line = m_keyboardLayout[row];

        for (size_t col = 0; col < line.Length(); ++col) {
            float xOffset = std::fmod((0.5 * row), 1.5);
            float x = (col + xOffset) * (keySize + m_keySpacing);

            // Special offset for row 3 (bottom row)
            if (row == 3) {
                x += 0.5f * keySize;
            }

            float y = row * (keySize + m_keySpacing);

            wxRect2DDouble keyRect(x, y, keySize, keySize);
            m_keys[keyIndex]->SetGeometry(keyRect);
            keyIndex++;
        }
    }

    // Position space bar
    if (m_spaceKey) {
        float spaceX = 2.5f * (keySize + m_keySpacing);
        float spaceY = m_keyboardLayout.size() * (keySize + m_keySpacing);
        float spaceWidth = 6 * keySize + 5 * m_keySpacing;

        wxRect2DDouble spaceRect(spaceX, spaceY, spaceWidth, keySize);
        m_spaceKey->SetGeometry(spaceRect);
    }
}

void KeyboardView::UpdateDwellProgress(KeyButton *key, float deltaMs)
{
    if (!key) return;

    float currentProgress = key->GetProgress();
    float progressIncrement = deltaMs / m_dwellTimeMs;
    key->SetProgress(currentProgress + progressIncrement);
}

KeyButton* KeyboardView::FindKeyAtPosition(const wxPoint2DDouble &pos)
{
    // Check space bar first (larger target)
    if (m_spaceKey && m_spaceKey->Contains(pos)) {
        return m_spaceKey;
    }

    // Check regular keys
    for (KeyButton* key : m_keys) {
        if (key->Contains(pos)) {
            return key;
        }
    }

    return nullptr;
}
