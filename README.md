# HeyEye Unified

A unified eye-tracking text input system combining the best features from HeyEyeControl and HeyEyeTracker projects.

## Overview

HeyEye Unified provides an intuitive eye-controlled interface with two complementary text input modes:

1. **Letter-by-Letter Mode**: Dwell-based selection of individual characters with visual progress feedback
2. **Swipe Mode**: ML-powered gesture recognition that predicts whole words from swipe paths

## Features

### Core Components

- **KeyButton**: Individual keyboard key with dwell-time progress indicator and multi-layer character display
- **KeyboardView**: Complete French AZERTY keyboard (4 rows + modifiers) with dual input mode support
- **GazeTracker**: Tobii eye tracking integration (~120Hz update rate) with mouse fallback mode
- **TextInputEngine**: ML pipeline integrating ONNX, FAISS, KenLM, and LightGBM for swipe prediction
- **EyeOverlay**: Full-screen transparent overlay with high-quality rendering (wxGraphicsContext + per-pixel alpha)
- **CircularButton**: Radial menu buttons for mouse control (click, drag, scroll) à la HeyEyeControl

### Keyboard Features

- **Multi-Layer Character Display**: Each key shows all available characters
  - Primary character (center, large, bold when active)
  - Shift character (top-left corner, dimmed when inactive)
  - AltGr character (top-right corner, dimmed when inactive)
- **Active Character Highlighting**: The character that will be typed is emphasized
  - Larger font (20pt vs 10pt)
  - Bold weight
  - Full brightness vs dimmed
  - Centered position
- **Complete AZERTY Layout**:
  - 4 rows of character keys with French accents (é, è, ç, à, ù)
  - Full modifier support (Shift, Caps Lock, AltGr)
  - Special characters (€, £, ¤, §, °, µ, ¨)
  - 47 character keys + modifiers + space bar
- **Visual Feedback**: Real-time progress arcs, hover highlighting, modifier states

### Input Modes

#### Letter-by-Letter Mode
- Hover over a key with your gaze
- Visual progress arc shows dwell time (800ms default)
- Active character is highlighted (bold + large + centered)
- Automatic selection when dwell time completes
- Modifier keys (Shift/Caps/AltGr) change which character is active

#### Swipe Mode
- Start gazing at the first letter
- Move your gaze across letters to form a word path
- Release (look away from keyboard) to complete gesture
- ML model predicts the intended word from the path
- Path visualization with dots and lines

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

- **wxWidgets 3.2+** (installed via vcpkg recommended)
- **CMake 3.15+**
- **C++17 compiler** (MSVC 2019+, GCC 9+, or Clang 9+)
- **vcpkg** (recommended for Windows dependency management)
- **Windows SDK** (for user32.lib - mouse/cursor control)

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

#### Quick Build (Windows)

```cmd
cd HeyEyeUnified
build.bat
```

This will:
1. Clean the build directory
2. Run CMake with vcpkg toolchain
3. Build Release configuration
4. Output: `build\Release\HeyEyeUnified.exe`

#### Manual Build

1. **Install wxWidgets via vcpkg**:
```cmd
D:\Deps\vcpkg\vcpkg.exe install wxwidgets:x64-windows
```

2. **Configure with CMake**:
```cmd
cd HeyEyeUnified
cmake -S . -B build ^
    -G "Visual Studio 16 2019" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=D:/Deps/vcpkg/scripts/buildsystems/vcpkg.cmake
```

3. **Build**:
```cmd
cmake --build build --config Release
```

4. **Run**:
```cmd
build\Release\HeyEyeUnified.exe
```

#### Optional: Enable ML Features

```cmd
D:\Deps\vcpkg\vcpkg.exe install faiss:x64-windows lightgbm:x64-windows

cmake -S . -B build ^
    -G "Visual Studio 16 2019" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=D:/Deps/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DUSE_ONNX=ON ^
    -DUSE_FAISS=ON ^
    -DUSE_KENLM=ON ^
    -DUSE_LIGHTGBM=ON ^
    -DUSE_TOBII=ON
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
| Framework | Qt 6.5 | Qt 6.5 | **wxWidgets 3.2** |
| Letter-by-letter input | ✓ (via EyePanel) | ✗ | ✓ |
| Swipe ML prediction | ✗ | ✓ | ✓ |
| Mode switching | ✗ | ✗ | ✓ |
| Clean class structure | Partial | Partial | ✓ |
| Full-screen overlay | ✓ | ✗ | ✓ (per-pixel alpha) |
| Visual keyboard | Basic (3 rows) | QLabel-based | **Full 4-row AZERTY** |
| Multi-layer display | ✗ | ✗ | **✓ (primary/shift/altgr)** |
| Active char highlighting | ✗ | ✗ | **✓ (bold + large)** |
| Progress indicator | ✓ | ✗ | ✓ |
| Modular architecture | ✗ | ✗ | ✓ |
| Mouse control (radial menu) | ✓ | ✗ | ✓ |
| Unicode support | Partial | Partial | **✓ (full Unicode)** |

## Testing Without Eye Tracker

The application can run without a Tobii device:

1. Set `m_manualMode = true` in `GazeTracker`
2. Use mouse to simulate gaze (enable mouse tracking)

## Known Limitations

- ML models require substantial RAM (FAISS index + ONNX models ~300MB-3GB)
- Swipe prediction requires pre-trained models and vocabulary
- Currently configured for French AZERTY layout only
- Optimized for Windows (transparent overlay, per-pixel alpha rendering)
- Eye tracking requires Tobii hardware (mouse mode available for testing)

## Recent Improvements (2025-10-29)

- ✅ **Merged Duplicate Keyboards**: Unified KeyboardView layout with EyeOverlay rendering
- ✅ **Multi-Layer Character Display**: All 3 character layers visible on each key
- ✅ **Active Character Highlighting**: Dynamic visual feedback based on modifier state
- ✅ **Unicode Character Support**: Fixed encoding issues for French accents (é, è, ç, à, ù)
- ✅ **Complete AZERTY Layout**: Full 4-row keyboard with 47 keys + modifiers
- ✅ **Improved Key Size**: 20% larger keys for better visibility (1600×500px keyboard area)
- ✅ **High-Quality Rendering**: wxGraphicsContext with per-pixel alpha transparency

## Future Enhancements

- [ ] Add QWERTY/QWERTZ layouts
- [ ] Implement adaptive dwell time based on user proficiency
- [ ] Add word suggestion bar above keyboard
- [ ] Implement settings dialog for customization (Settings class exists)
- [ ] Add calibration routine for eye tracker
- [ ] Support for multiple languages (currently French)
- [ ] Auto-capitalization and punctuation prediction
- [ ] Swipe path smoothing and noise reduction
- [ ] Cross-platform support (Linux, macOS)

## License

This project combines components from HeyEyeControl and HeyEyeTracker. Refer to the original projects for licensing information.

## Credits

Built on top of:
- HeyEyeControl - Eye gaze overlay and control interface
- HeyEyeTracker - ML-based swipe prediction system
- Tobii Stream Engine SDK
- ONNX Runtime, FAISS, KenLM, LightGBM
