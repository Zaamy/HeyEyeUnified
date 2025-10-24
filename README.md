# HeyEye Unified

A unified eye-tracking text input system combining the best features from HeyEyeControl and HeyEyeTracker projects.

## Overview

HeyEye Unified provides an intuitive eye-controlled interface with two complementary text input modes:

1. **Letter-by-Letter Mode**: Dwell-based selection of individual characters with visual progress feedback
2. **Swipe Mode**: ML-powered gesture recognition that predicts whole words from swipe paths

## Features

### Core Components

- **KeyButton**: Individual keyboard key with dwell-time progress indicator and swipe highlighting
- **KeyboardView**: AZERTY French keyboard layout with dual input mode support
- **GazeTracker**: Tobii eye tracking integration (~120Hz update rate)
- **TextInputEngine**: ML pipeline integrating ONNX, FAISS, KenLM, and LightGBM
- **EyeOverlay**: Full-screen transparent interface with stay-on-top behavior

### Input Modes

#### Letter-by-Letter Mode
- Hover over a key with your gaze
- Visual progress arc shows dwell time
- Automatic selection when dwell time completes
- Default dwell time: 800ms (configurable)

#### Swipe Mode
- Start gazing at the first letter
- Move your gaze across letters to form a word path
- Release (look away from keyboard) to complete gesture
- ML model predicts the intended word from the path

### ML Pipeline (Swipe Mode)

The swipe prediction pipeline consists of:

1. **Swipe Encoder** (ONNX): Converts gaze path to 128-dimensional embedding
2. **FAISS Index**: Retrieves top-100 candidate words via vector similarity
3. **Feature Computation**: Calculates 70+ features per candidate:
   - DTW distance (Dynamic Time Warping between swipe and ideal keyboard path)
   - Language model score (KenLM n-gram probability)
   - Normalized, z-score, percentile, and interaction features
4. **LightGBM Ranker**: Ranks candidates and selects best prediction

## Architecture

```
HeyEyeUnified/
├── include/
│   ├── keybutton.h           # Individual key with progress visualization
│   ├── keyboardview.h        # Full keyboard with dual input modes
│   ├── gazetracker.h         # Tobii eye tracking integration
│   ├── textinputengine.h     # ML prediction pipeline
│   └── eyeoverlay.h          # Main UI overlay
├── src/
│   ├── keybutton.cpp
│   ├── keyboardview.cpp
│   ├── gazetracker.cpp
│   ├── textinputengine.cpp
│   ├── eyeoverlay.cpp
│   └── main.cpp
├── assets/                   # ML models and data (not included)
│   ├── swipe_encoder.onnx
│   ├── vocab.msgpck
│   ├── index.faiss
│   └── lightgbm_model.txt
└── HeyEyeUnified.pro         # Qt project file
```

## Building

### Requirements

- Qt 6.5.3 or later (Core, GUI, Widgets, Multimedia modules)
- C++17 compiler
- CMake (for building dependencies)

### Optional ML Dependencies

For full swipe prediction functionality:

- **Tobii Stream Engine SDK** - Eye tracking hardware interface
- **ONNX Runtime 1.19.2+** - Neural network inference
- **FAISS** - Vector similarity search (install via vcpkg)
- **KenLM** - Language model scoring
- **LightGBM** - Learning-to-rank (install via vcpkg)
- **espeak-ng** - Text-to-speech (optional)
- **llama.cpp** - LLM integration (optional)

### Build Steps

1. **Install vcpkg dependencies** (if using ML features):
```bash
vcpkg install faiss:x64-windows lightgbm:x64-windows
```

2. **Build submodules** (if using):
```bash
# espeak-ng
cd ../to_combined/HeyEyeTracker/espeak-ng
mkdir build && cd build
cmake .. && cmake --build . --config Release

# llama.cpp
cd ../../llama.cpp
mkdir build && cd build
cmake .. && cmake --build . --config Release
```

3. **Open in Qt Creator**:
   - Open `HeyEyeUnified.pro`
   - Configure build paths in the .pro file
   - Build (Ctrl+B)

4. **Copy assets** (if available):
```bash
mkdir build/release/assets
cp ../to_combined/HeyEyeTracker/assets/* build/release/assets/
```

## Configuration

Edit `HeyEyeUnified.pro` to set dependency paths:

```qmake
# Example: Enable ONNX Runtime
LIBS += -L/path/to/onnxruntime/lib -lonnxruntime
INCLUDEPATH += /path/to/onnxruntime/include
```

Uncomment and configure the sections for each dependency you want to use.

## Usage

### Keyboard Shortcuts

- `K` - Toggle keyboard visibility
- `M` - Switch between Letter-by-Letter and Swipe modes
- `ESC` - Exit application

### Basic Operation

1. Launch the application
2. The overlay appears transparent across your screen
3. Press `K` to show the keyboard
4. Choose your input mode:
   - **Letter-by-Letter**: Look at each letter until the progress arc completes
   - **Swipe**: Look at the starting letter, trace the word path with your gaze, then look away

### Control Buttons

- **Show/Hide Keyboard** - Toggle keyboard visibility
- **Switch Mode** - Toggle between input modes
- **Delete Word** - Remove the last word entered
- **Speak** - Text-to-speech output (requires espeak-ng)

## Development

### Adding ML Model Integration

The current implementation provides the architecture and interfaces but requires linking against the actual ML libraries. To add full functionality:

1. **Implement `TextInputEngine::loadSwipeEncoder()`**:
   - Load ONNX model using ONNX Runtime
   - Set up input/output tensors

2. **Implement `TextInputEngine::encodeSwipe()`**:
   - Preprocess swipe path
   - Run ONNX inference
   - Return embedding vector

3. **Implement vocabulary and FAISS loading**:
   - Parse MessagePack vocabulary
   - Load FAISS index from file

4. **Implement ranking pipeline**:
   - Copy feature computation from `HeyEyeTracker/ranking_features.cpp`
   - Integrate LightGBM prediction

See `INTEGRATION_GUIDE.md` for detailed integration steps.

### Class Hierarchy

```
QObject
├── KeyButton           - Single keyboard key
├── GazeTracker        - Eye tracking manager
└── TextInputEngine    - ML prediction pipeline

QWidget
├── KeyboardView       - Keyboard widget with all keys
└── EyeOverlay         - Main application window
```

### Key Design Patterns

- **Signal/Slot Communication**: All inter-component communication via Qt signals
- **Forward Declarations**: Minimize header dependencies for faster compilation
- **Pimpl Pattern**: Hide ML library details (ready for use with LightGBM)
- **Mode Pattern**: `InputMode` enum for behavioral switching

## Comparison with Original Projects

| Feature | HeyEyeControl | HeyEyeTracker | HeyEye Unified |
|---------|---------------|---------------|----------------|
| Letter-by-letter input | ✓ (via EyePanel) | ✗ | ✓ |
| Swipe ML prediction | ✗ | ✓ | ✓ |
| Mode switching | ✗ | ✗ | ✓ |
| Clean class structure | Partial | Partial | ✓ |
| Full-screen overlay | ✓ | ✗ | ✓ |
| Visual keyboard | Basic | QLabel-based | KeyButton-based |
| Progress indicator | ✓ | ✗ | ✓ |
| Modular architecture | ✗ | ✗ | ✓ |

## Testing Without Eye Tracker

The application can run without a Tobii device:

1. Set `m_manualMode = true` in `GazeTracker`
2. Use mouse to simulate gaze (enable mouse tracking)
3. Test keyboard shortcuts and mode switching

## Known Limitations

- ML models require substantial RAM (FAISS index + ONNX models)
- Swipe prediction requires pre-trained models and vocabulary
- Currently configured for French AZERTY layout only
- Requires Windows for full overlay functionality (uiAccess)

## Future Enhancements

- [ ] Add QWERTY/QWERTZ layouts
- [ ] Implement adaptive dwell time based on user proficiency
- [ ] Add word suggestion bar above keyboard
- [ ] Implement settings dialog for customization
- [ ] Add calibration routine for eye tracker
- [ ] Support for multiple languages
- [ ] Auto-capitalization and punctuation prediction
- [ ] Swipe path smoothing and noise reduction

## License

This project combines components from HeyEyeControl and HeyEyeTracker. Refer to the original projects for licensing information.

## Credits

Built on top of:
- HeyEyeControl - Eye gaze overlay and control interface
- HeyEyeTracker - ML-based swipe prediction system
- Tobii Stream Engine SDK
- ONNX Runtime, FAISS, KenLM, LightGBM
