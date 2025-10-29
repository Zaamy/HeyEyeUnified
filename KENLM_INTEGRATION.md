# KenLM Integration in HeyEyeUnified

## Overview

KenLM is now integrated into HeyEyeUnified's `TextInputEngine` for N-gram language model scoring. This provides statistical language modeling capabilities similar to the HeyEyeTracker implementation.

## Features Implemented

### 1. **Model Loading**
- `LoadKenLM(const wxString& modelPath)` - Loads a KenLM binary model file
- Supports models up to order 6 (KENLM_MAX_ORDER=6)
- Exception handling for model loading failures

### 2. **Sequence Evaluation**
- `EvaluateSequence(const std::vector<wxString>& words)` - Scores a complete word sequence
- Returns log probability score for the sequence
- Automatically adds begin/end sentence markers

### 3. **Incremental Evaluation with State**
- `EvaluateIncremental(words, initialLogProb, initialState)` - Scores words with context state
- Returns both the log probability and the final state
- Useful for ranking multiple candidate words in context
- Compatible with HeyEyeTracker's ranking pipeline

### 4. **State Management**
- `GetBeginSentenceState()` - Gets initial state for sentence beginning
- State objects maintain language model context between evaluations
- Allows efficient incremental scoring of candidate words

## Build Configuration

### Prerequisites

1. **KenLM Library**
   - Location: `D:/Deps/kenlm/`
   - Binary library: `D:/Deps/kenlm/windows/kenlm_x64.lib`

2. **Boost Libraries** (required by KenLM)
   - Install via vcpkg: `vcpkg install boost:x64-windows`
   - Required components: system, filesystem, thread, program_options

### CMake Build Options

Enable KenLM support during CMake configuration:

```cmd
cmake -S . -B build ^
    -G "Visual Studio 16 2019" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=D:/Deps/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DUSE_KENLM=ON
```

### Full Build with KenLM

```cmd
cd D:\workspace\HeyEye\HeyEyeUnified
rmdir /s /q build
cmake -S . -B build ^
    -G "Visual Studio 16 2019" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=D:/Deps/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DUSE_KENLM=ON
cmake --build build --config Release
```

## Usage Examples

### Example 1: Simple Sequence Scoring

```cpp
#include "textinputengine.h"

TextInputEngine engine;
engine.Initialize("./assets");

// Score a sentence
std::vector<wxString> words = {wxT("hello"), wxT("world")};
float score = engine.EvaluateSequence(words);
wxLogMessage("Sentence score: %f", score);
```

### Example 2: Incremental Candidate Ranking

```cpp
// Get initial state for an empty context
void* initialState = engine.GetBeginSentenceState();
float initialLogProb = 0.0f;

// Previous words in the sentence
std::vector<wxString> context = {wxT("the"), wxT("quick")};

// Evaluate context
auto contextResult = engine.EvaluateIncremental(context, initialLogProb, initialState);

// Now score each candidate word in context
std::vector<wxString> candidates = {wxT("brown"), wxT("red"), wxT("blue")};
for (const wxString& candidate : candidates) {
    std::vector<wxString> candidateWords = {candidate};
    auto result = engine.EvaluateIncremental(candidateWords,
                                            contextResult.logProb,
                                            contextResult.state);
    wxLogMessage("%s: score = %f", candidate, result.logProb);
}
```

### Example 3: Ranking Pipeline (like HeyEyeTracker)

```cpp
// Start with sentence context
std::vector<wxString> history = {wxT("je"), wxT("vais")};

void* state = engine.GetBeginSentenceState();
float logProb = 0.0f;

// Score history
auto historyResult = engine.EvaluateIncremental(history, logProb, state);

// Get candidates from FAISS/vocabulary search
std::vector<wxString> candidates = GetCandidatesFromFAISS(...);

// Rank each candidate
std::vector<std::pair<wxString, float>> rankedCandidates;
for (const wxString& candidate : candidates) {
    auto result = engine.EvaluateIncremental({candidate},
                                            historyResult.logProb,
                                            historyResult.state);
    rankedCandidates.push_back({candidate, result.logProb});
}

// Sort by score (higher is better)
std::sort(rankedCandidates.begin(), rankedCandidates.end(),
          [](const auto& a, const auto& b) { return a.second > b.second; });

wxString bestWord = rankedCandidates[0].first;
```

## Model File Format

KenLM expects binary model files with `.bin` extension:
- **Format**: KenLM binary (compiled with `build_binary` tool)
- **Max Order**: Up to 6-gram (KENLM_MAX_ORDER=6)
- **Example path**: `assets/kenlm_model.bin`

### Training Your Own Model

Use KenLM tools to train a model from text corpus:

```bash
# Count n-grams
lmplz -o 5 < corpus.txt > model.arpa

# Convert to binary format
build_binary model.arpa model.bin
```

## Implementation Details

### File Changes

1. **textinputengine.h** (include/textinputengine.h:64-72)
   - Added `EvaluateSequence()` method
   - Added `EvaluateIncremental()` method with KenLMResult struct
   - Added `GetBeginSentenceState()` method

2. **textinputengine.cpp** (src/textinputengine.cpp)
   - Implemented `LoadKenLM()` with exception handling
   - Implemented `EvaluateSequence()` for complete sentence scoring
   - Implemented `EvaluateIncremental()` for stateful evaluation
   - Implemented `GetBeginSentenceState()` for state initialization
   - Added `#include "lm/model.hh"` under `#ifdef USE_KENLM`

3. **CMakeLists.txt** (CMakeLists.txt:116-162)
   - Added `USE_KENLM` option
   - Added KenLM library finding and linking
   - Added Boost dependency finding and linking
   - Added `KENLM_MAX_ORDER=6` compile definition

### Thread Safety

The current implementation is **not thread-safe**. If you need concurrent access:
- Create separate `TextInputEngine` instances per thread
- OR add mutex protection around KenLM calls

### Memory Management

- KenLM State objects are heap-allocated via `new`
- The caller is responsible for deallocating state objects returned by `EvaluateIncremental()`
- The `m_kenLM` model is deleted in `TextInputEngine` destructor

## Comparison with HeyEyeTracker

| Feature | HeyEyeTracker | HeyEyeUnified |
|---------|---------------|---------------|
| Model Loading | `kenlm_init()` | `LoadKenLM()` |
| Evaluation | `kenlm_evaluate(QStringList, float, State)` | `EvaluateIncremental(vector<wxString>, float, void*)` |
| Simple Scoring | ❌ | `EvaluateSequence(vector<wxString>)` |
| State Management | Manual | `GetBeginSentenceState()` helper |
| String Type | QString | wxString |
| Header Include | Direct `lm/model.hh` | Conditional `#ifdef USE_KENLM` |
| Max Order | Hardcoded 6 | CMake define `KENLM_MAX_ORDER` |

## Troubleshooting

### Error: "KenLM model not found"
- Ensure model file exists at the specified path
- Check file permissions
- Verify model is in KenLM binary format (not ARPA text)

### Error: "Boost not found"
```cmd
# Install Boost via vcpkg
D:\Deps\vcpkg\vcpkg.exe install boost:x64-windows

# Then reconfigure CMake
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=D:/Deps/vcpkg/scripts/buildsystems/vcpkg.cmake -DUSE_KENLM=ON
```

### Error: "kenlm_x64.lib not found"
- Check if library exists at `D:/Deps/kenlm/windows/kenlm_x64.lib`
- Verify path in CMakeLists.txt matches your installation
- Rebuild KenLM if necessary

### Link Errors with Boost
- Ensure all Boost components are installed: system, filesystem, thread, program_options
- Check that Boost version is compatible with KenLM
- Try using static Boost libraries if dynamic linking fails

## Future Enhancements

1. **Vocabulary Filtering**: Filter candidates to only in-vocabulary words
2. **OOV Handling**: Improve out-of-vocabulary word handling
3. **Model Caching**: Cache multiple models for different languages
4. **Performance**: Profile and optimize state management
5. **Thread Safety**: Add mutex protection for concurrent access

## References

- **KenLM**: https://kheafield.com/code/kenlm/
- **HeyEyeTracker Implementation**: `to_combined/HeyEyeTracker/kenlm.cpp`
- **Language Model Paper**: "KenLM: Faster and Smaller Language Model Queries" (Kenneth Heafield, 2011)
