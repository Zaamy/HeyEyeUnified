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
    , m_shiftKey(nullptr)
    , m_capsLockKey(nullptr)
    , m_altgrKey(nullptr)
    , m_backspaceKey(nullptr)
    , m_enterKey(nullptr)
    , m_shiftActive(false)
    , m_capsLockActive(false)
    , m_altgrActive(false)
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
    , OnBackspacePressed(nullptr)
    , OnEnterPressed(nullptr)
{
    // Set background color (dark gray)
    SetBackgroundColour(wxColour(50, 50, 50));

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
    delete m_shiftKey;
    delete m_capsLockKey;
    delete m_altgrKey;
    delete m_backspaceKey;
    delete m_enterKey;
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

    // Coordinates are already in keyboard-local space (converted by EyeOverlay)
    m_gazePosition = wxPoint2DDouble(x, y);

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
        coords[key->GetPrimaryCharacter()] = std::make_pair(centerX, centerY);
    }

    if (m_spaceKey) {
        wxRect2DDouble rect = m_spaceKey->GetGeometry();
        float centerX = static_cast<float>(rect.m_x + rect.m_width / 2);
        float centerY = static_cast<float>(rect.m_y + rect.m_height / 2);
        coords[wxT(' ')] = std::make_pair(centerX, centerY);
    }

    return coords;
}

void KeyboardView::RenderToDC(wxDC& dc)
{
    // Clear background
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();

    // Draw all keys
    for (KeyButton* key : m_keys) {
        key->Draw(dc, m_normalColor, m_hoverColor, m_progressColor, m_shiftActive, m_capsLockActive, m_altgrActive);
    }

    // Draw space bar
    if (m_spaceKey) {
        m_spaceKey->Draw(dc, m_normalColor, m_hoverColor, m_progressColor, m_shiftActive, m_capsLockActive, m_altgrActive);
    }

    // Draw modifier keys
    if (m_shiftKey) {
        m_shiftKey->Draw(dc, m_normalColor, m_hoverColor, m_progressColor, m_shiftActive, m_capsLockActive, m_altgrActive);
    }
    if (m_capsLockKey) {
        m_capsLockKey->Draw(dc, m_normalColor, m_hoverColor, m_progressColor, m_shiftActive, m_capsLockActive, m_altgrActive);
    }
    if (m_altgrKey) {
        m_altgrKey->Draw(dc, m_normalColor, m_hoverColor, m_progressColor, m_shiftActive, m_capsLockActive, m_altgrActive);
    }
    if (m_backspaceKey) {
        m_backspaceKey->Draw(dc, m_normalColor, m_hoverColor, m_progressColor, m_shiftActive, m_capsLockActive, m_altgrActive);
    }
    if (m_enterKey) {
        m_enterKey->Draw(dc, m_normalColor, m_hoverColor, m_progressColor, m_shiftActive, m_capsLockActive, m_altgrActive);
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

void KeyboardView::OnPaint(wxPaintEvent& event)
{
    wxUnusedVar(event);

    // Use buffered paint DC for smooth rendering
    wxBufferedPaintDC dc(this);

    // Render using common method
    RenderToDC(dc);
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

    // Define complete French AZERTY layout with primary, shift, and AltGr characters
    // Format: {primary, shift, altgr}
    // Use Unicode escape sequences for proper encoding

    // Row 0 - Numbers and symbols
    struct KeyDef { wxChar primary; wxChar shift; wxChar altgr; };
    std::vector<std::vector<KeyDef>> layout = {
        // Row 0: &é"'(-è_çà)=
        {
            {L'&', L'1', L'\0'},
            {L'\u00E9', L'2', L'~'},  // é
            {L'"', L'3', L'#'},
            {L'\'', L'4', L'{'},
            {L'(', L'5', L'['},
            {L'-', L'6', L'|'},
            {L'\u00E8', L'7', L'`'},  // è
            {L'_', L'8', L'\\'},
            {L'\u00E7', L'9', L'^'},  // ç
            {L'\u00E0', L'0', L'@'},  // à
            {L')', L'\u00B0', L']'},  // °
            {L'=', L'+', L'}'},
        },
        // Row 1: azertyuiop^$
        {
            {L'a', L'A', L'\0'},
            {L'z', L'Z', L'\0'},
            {L'e', L'E', L'\u20AC'},  // €
            {L'r', L'R', L'\0'},
            {L't', L'T', L'\0'},
            {L'y', L'Y', L'\0'},
            {L'u', L'U', L'\0'},
            {L'i', L'I', L'\0'},
            {L'o', L'O', L'\0'},
            {L'p', L'P', L'\0'},
            {L'^', L'\u00A8', L'\0'},  // ¨
            {L'$', L'\u00A3', L'\u00A4'},  // £ ¤
        },
        // Row 2: qsdfghjklmù*
        {
            {L'q', L'Q', L'\0'},
            {L's', L'S', L'\0'},
            {L'd', L'D', L'\0'},
            {L'f', L'F', L'\0'},
            {L'g', L'G', L'\0'},
            {L'h', L'H', L'\0'},
            {L'j', L'J', L'\0'},
            {L'k', L'K', L'\0'},
            {L'l', L'L', L'\0'},
            {L'm', L'M', L'\0'},
            {L'\u00F9', L'%', L'\0'},  // ù
            {L'*', L'\u00B5', L'\0'},  // µ
        },
        // Row 3: <wxcvbn,;:!
        {
            {L'<', L'>', L'\0'},
            {L'w', L'W', L'\0'},
            {L'x', L'X', L'\0'},
            {L'c', L'C', L'\0'},
            {L'v', L'V', L'\0'},
            {L'b', L'B', L'\0'},
            {L'n', L'N', L'\0'},
            {L',', L'?', L'\0'},
            {L';', L'.', L'\0'},
            {L':', L'/', L'\0'},
            {L'!', L'\u00A7', L'\0'},  // §
        }
    };

    // Create keys from layout
    for (size_t row = 0; row < layout.size(); ++row) {
        for (size_t col = 0; col < layout[row].size(); ++col) {
            const KeyDef& keyDef = layout[row][col];
            KeyButton* key = new KeyButton(keyDef.primary, keyDef.shift, keyDef.altgr, wxRect2DDouble());

            // Set up activation callback
            key->OnActivated = [this, key]() {
                if (m_inputMode == InputMode::LetterByLetter && OnLetterSelected) {
                    wxChar effectiveChar = GetEffectiveCharacter(key);
                    if (effectiveChar != 0) {
                        OnLetterSelected(effectiveChar);
                        // Reset shift after selection (but not caps lock)
                        if (m_shiftActive && !m_capsLockActive) {
                            ToggleShift();
                        }
                    }
                    // Reset after selection
                    if (m_currentHoveredKey) {
                        m_currentHoveredKey->SetProgress(0.0f);
                    }
                }
            };

            m_keys.push_back(key);
            m_keyMap[keyDef.primary] = key;
        }
    }

    // Create space bar
    m_spaceKey = new KeyButton(L' ', L' ', L'\0', wxRect2DDouble());
    m_spaceKey->OnActivated = [this]() {
        if (m_inputMode == InputMode::LetterByLetter && OnSpacePressed) {
            OnSpacePressed();
            if (m_currentHoveredKey == m_spaceKey) {
                m_spaceKey->SetProgress(0.0f);
            }
        }
    };

    // Create modifier keys
    m_shiftKey = new KeyButton(KeyType::Shift, wxT("Shift"), wxRect2DDouble());
    m_shiftKey->OnActivated = [this]() {
        if (m_inputMode == InputMode::LetterByLetter) {
            ToggleShift();
            if (m_currentHoveredKey == m_shiftKey) {
                m_shiftKey->SetProgress(0.0f);
            }
        }
    };

    m_capsLockKey = new KeyButton(KeyType::CapsLock, wxT("Caps"), wxRect2DDouble());
    m_capsLockKey->OnActivated = [this]() {
        if (m_inputMode == InputMode::LetterByLetter) {
            ToggleCapsLock();
            if (m_currentHoveredKey == m_capsLockKey) {
                m_capsLockKey->SetProgress(0.0f);
            }
        }
    };

    m_altgrKey = new KeyButton(KeyType::AltGr, wxT("AltGr"), wxRect2DDouble());
    m_altgrKey->OnActivated = [this]() {
        if (m_inputMode == InputMode::LetterByLetter) {
            ToggleAltGr();
            if (m_currentHoveredKey == m_altgrKey) {
                m_altgrKey->SetProgress(0.0f);
            }
        }
    };

    m_backspaceKey = new KeyButton(KeyType::Backspace, wxT("⌫"), wxRect2DDouble());
    m_backspaceKey->OnActivated = [this]() {
        if (m_inputMode == InputMode::LetterByLetter && OnBackspacePressed) {
            OnBackspacePressed();
            if (m_currentHoveredKey == m_backspaceKey) {
                m_backspaceKey->SetProgress(0.0f);
            }
        }
    };

    m_enterKey = new KeyButton(KeyType::Enter, wxT("Enter"), wxRect2DDouble());
    m_enterKey->OnActivated = [this]() {
        if (m_inputMode == InputMode::LetterByLetter && OnEnterPressed) {
            OnEnterPressed();
            if (m_currentHoveredKey == m_enterKey) {
                m_enterKey->SetProgress(0.0f);
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

    // Define rows layout (number of keys per row)
    std::vector<size_t> rowSizes = {12, 12, 12, 11};  // Row 0-3

    // Calculate key size based on widget dimensions
    // Need space for 12 keys in the longest row + spacing
    // Increased key spacing for better visibility of multi-layer characters
    float availableWidth = clientSize.GetWidth() - m_keySpacing * 13;
    float keyWidth = availableWidth / 12.0f;

    // Need space for 6 rows (4 regular + 1 space/modifiers + padding)
    float availableHeight = clientSize.GetHeight() - m_keySpacing * 7;
    float keyHeight = availableHeight / 6.0f;

    // Increase key size by 20% for better multi-character visibility
    float keySize = std::min(keyWidth, keyHeight) * 1.2f;

    // Position regular keys in 4 rows
    size_t keyIndex = 0;
    for (size_t row = 0; row < rowSizes.size(); ++row) {
        size_t numKeys = rowSizes[row];

        for (size_t col = 0; col < numKeys; ++col) {
            float xOffset = std::fmod((0.5f * row), 1.5f);
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

    // Row 4: Space bar and modifier keys
    float row4Y = 4 * (keySize + m_keySpacing);

    // Left side: Shift, Caps, AltGr (1 key width each)
    if (m_shiftKey) {
        float x = 0;
        wxRect2DDouble rect(x, row4Y, keySize, keySize);
        m_shiftKey->SetGeometry(rect);
    }

    if (m_capsLockKey) {
        float x = (keySize + m_keySpacing);
        wxRect2DDouble rect(x, row4Y, keySize, keySize);
        m_capsLockKey->SetGeometry(rect);
    }

    if (m_altgrKey) {
        float x = 2 * (keySize + m_keySpacing);
        wxRect2DDouble rect(x, row4Y, keySize, keySize);
        m_altgrKey->SetGeometry(rect);
    }

    // Center: Space bar (5 keys width)
    if (m_spaceKey) {
        float spaceX = 3 * (keySize + m_keySpacing);
        float spaceWidth = 5 * keySize + 4 * m_keySpacing;
        wxRect2DDouble spaceRect(spaceX, row4Y, spaceWidth, keySize);
        m_spaceKey->SetGeometry(spaceRect);
    }

    // Right side: Backspace, Enter (1.5 key width each)
    if (m_backspaceKey) {
        float x = 8 * (keySize + m_keySpacing);
        float width = 1.5f * keySize + 0.5f * m_keySpacing;
        wxRect2DDouble rect(x, row4Y, width, keySize);
        m_backspaceKey->SetGeometry(rect);
    }

    if (m_enterKey) {
        float x = 9.5f * (keySize + m_keySpacing) + 0.5f * keySize;
        float width = 1.5f * keySize + 0.5f * m_keySpacing;
        wxRect2DDouble rect(x, row4Y, width, keySize);
        m_enterKey->SetGeometry(rect);
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

    // Check modifier keys
    if (m_shiftKey && m_shiftKey->Contains(pos)) {
        return m_shiftKey;
    }
    if (m_capsLockKey && m_capsLockKey->Contains(pos)) {
        return m_capsLockKey;
    }
    if (m_altgrKey && m_altgrKey->Contains(pos)) {
        return m_altgrKey;
    }
    if (m_backspaceKey && m_backspaceKey->Contains(pos)) {
        return m_backspaceKey;
    }
    if (m_enterKey && m_enterKey->Contains(pos)) {
        return m_enterKey;
    }

    // Check regular keys
    for (KeyButton* key : m_keys) {
        if (key->Contains(pos)) {
            return key;
        }
    }

    return nullptr;
}

void KeyboardView::ToggleShift()
{
    m_shiftActive = !m_shiftActive;
    if (m_shiftKey) {
        m_shiftKey->SetModifierActive(m_shiftActive);
    }
    Refresh();
}

void KeyboardView::ToggleCapsLock()
{
    m_capsLockActive = !m_capsLockActive;
    // CapsLock overrides Shift for letter keys
    if (m_capsLockActive) {
        m_shiftActive = false;
        if (m_shiftKey) {
            m_shiftKey->SetModifierActive(false);
        }
    }
    if (m_capsLockKey) {
        m_capsLockKey->SetModifierActive(m_capsLockActive);
    }
    Refresh();
}

void KeyboardView::ToggleAltGr()
{
    m_altgrActive = !m_altgrActive;
    if (m_altgrKey) {
        m_altgrKey->SetModifierActive(m_altgrActive);
    }
    Refresh();
}

wxChar KeyboardView::GetEffectiveCharacter(KeyButton* key)
{
    if (!key || key->GetKeyType() != KeyType::Character) {
        return 0;
    }

    // AltGr takes precedence
    if (m_altgrActive && key->GetAltGrCharacter() != 0) {
        return key->GetAltGrCharacter();
    }

    // Then Shift or Caps Lock
    if ((m_shiftActive || m_capsLockActive) && key->GetShiftCharacter() != 0) {
        return key->GetShiftCharacter();
    }

    // Default to primary
    return key->GetPrimaryCharacter();
}

std::vector<KeyRenderInfo> KeyboardView::GetKeysForRendering() const
{
    std::vector<KeyRenderInfo> renderInfo;

    // Helper lambda to create render info from a key
    auto makeRenderInfo = [this](KeyButton* key) -> KeyRenderInfo {
        KeyRenderInfo info;
        info.geometry = key->GetGeometry();
        info.progress = key->GetProgress();
        info.isHovered = key->IsHovered();
        info.isHighlighted = key->IsHighlighted();
        info.isModifierActive = key->IsModifierActive();
        info.keyType = key->GetKeyType();

        if (key->GetKeyType() == KeyType::Character) {
            // For character keys, show ALL three layers (primary, shift, altgr)
            // This allows users to see what characters are available on each key

            // Primary character (center/bottom)
            wxChar primaryChar = key->GetPrimaryCharacter();
            if (primaryChar != 0) {
                info.primaryLabel = wxString(&primaryChar, 1);
            }

            // Shift character (top-left)
            wxChar shiftChar = key->GetShiftCharacter();
            if (shiftChar != 0 && shiftChar != primaryChar) {
                info.shiftLabel = wxString(&shiftChar, 1);
            }

            // AltGr character (top-right)
            wxChar altgrChar = key->GetAltGrCharacter();
            if (altgrChar != 0) {
                info.altgrLabel = wxString(&altgrChar, 1);
            }

            // Determine which character layer is currently active
            if (m_altgrActive && altgrChar != 0) {
                info.activeLayer = KeyRenderInfo::AltGr;
            } else if ((m_shiftActive || m_capsLockActive) && shiftChar != 0) {
                info.activeLayer = KeyRenderInfo::Shift;
            } else {
                info.activeLayer = KeyRenderInfo::Primary;
            }
        } else {
            // For modifier keys, use the label directly in primary
            info.primaryLabel = key->GetLabel();
            info.activeLayer = KeyRenderInfo::Primary;
        }

        return info;
    };

    // Add all regular keys
    for (KeyButton* key : m_keys) {
        renderInfo.push_back(makeRenderInfo(key));
    }

    // Add space bar
    if (m_spaceKey) {
        renderInfo.push_back(makeRenderInfo(m_spaceKey));
    }

    // Add modifier keys
    if (m_shiftKey) {
        renderInfo.push_back(makeRenderInfo(m_shiftKey));
    }
    if (m_capsLockKey) {
        renderInfo.push_back(makeRenderInfo(m_capsLockKey));
    }
    if (m_altgrKey) {
        renderInfo.push_back(makeRenderInfo(m_altgrKey));
    }
    if (m_backspaceKey) {
        renderInfo.push_back(makeRenderInfo(m_backspaceKey));
    }
    if (m_enterKey) {
        renderInfo.push_back(makeRenderInfo(m_enterKey));
    }

    return renderInfo;
}
