# Required ML Assets

To enable full ML functionality in HeyEyeUnified, copy the following assets from HeyEyeTracker:

## Files to Copy

From `D:\workspace\HeyEye\to_combined\HeyEyeTracker\assets\` to `D:\workspace\HeyEye\HeyEyeUnified\assets\`:

1. **swipe_encoder.onnx** (ONNX model file - main model)
   - Size: ~few MB
   - Required for: Swipe gesture encoding to embeddings

2. **swipe_encoder.onnx.data** (ONNX model weights - if separate)
   - Size: ~few MB
   - Required for: ONNX model weights (companion file)

3. **vocab.msgpck** (MessagePack vocabulary file)
   - Size: ~few MB
   - Required for: Word vocabulary and embeddings mapping

4. **index.faiss** (FAISS vector index)
   - Size: ~tens of MB
   - Required for: Fast vector similarity search

5. **kenlm_model.bin** (KenLM language model - optional)
   - Location: You need to provide your own KenLM model
   - Required for: Language model scoring
   - Note: Not in HeyEyeTracker assets, needs separate training

6. **lightgbm_model.txt** (LightGBM ranker model - optional)
   - Location: You need to provide your own LightGBM model
   - Required for: Final ranking of word candidates
   - Note: Train using `to_combined/HeyEyeTracker/training/` scripts

## Copy Commands (Windows)

```cmd
REM From the HeyEye directory:
copy to_combined\HeyEyeTracker\assets\swipe_encoder.onnx HeyEyeUnified\assets\
copy to_combined\HeyEyeTracker\assets\swipe_encoder.onnx.data HeyEyeUnified\assets\
copy to_combined\HeyEyeTracker\assets\vocab.msgpck HeyEyeUnified\assets\
copy to_combined\HeyEyeTracker\assets\index.faiss HeyEyeUnified\assets\
```

## Build Configuration

After copying assets, configure CMake with ML features enabled:

```cmd
cd HeyEyeUnified
cmake -S . -B build ^
    -G "Visual Studio 16 2019" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=D:/Deps/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DUSE_ONNX=ON ^
    -DUSE_FAISS=ON ^
    -DUSE_KENLM=ON ^
    -DUSE_LIGHTGBM=ON ^
    -DUSE_MSGPACK=ON ^
    -DUSE_TOBII=ON

cmake --build build --config Release
```

## Required vcpkg Packages

Before building, ensure these packages are installed via vcpkg:

```cmd
D:\Deps\vcpkg\vcpkg.exe install wxwidgets:x64-windows
D:\Deps\vcpkg\vcpkg.exe install faiss:x64-windows
D:\Deps\vcpkg\vcpkg.exe install lightgbm:x64-windows
D:\Deps\vcpkg\vcpkg.exe install msgpack-cxx:x64-windows
```

## Runtime Requirements

The built executable will look for assets in:
- `<executable_directory>/assets/`

Make sure assets are in the correct location when running the application.
