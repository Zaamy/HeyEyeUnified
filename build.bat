@echo off
REM HeyEyeUnified Build Script for Windows
REM Requires: CMake, wxWidgets (via vcpkg), Visual Studio 2019

echo ====================================
echo HeyEyeUnified - wxWidgets + CMake Build
echo ====================================
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo [1/3] Configuring with CMake...
cmake .. -G "Visual Studio 16 2019" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=D:/Deps/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DUSE_TOBII=ON ^
    -DUSE_ONNX=ON ^
    -DUSE_FAISS=ON ^
    -DUSE_KENLM=ON ^
    -DUSE_LIGHTGBM=ON ^
    -DUSE_MSGPACK=ON ^
    -DUSE_ESPEAK=ON


if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

echo.
echo [2/3] Building...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

echo.
echo [3/3] Build complete!
echo.
echo Executable: build\bin\Release\HeyEyeUnified.exe
echo.
echo To enable ML features, edit this script and set:
echo   -DUSE_ONNX=ON -DUSE_FAISS=ON -DUSE_KENLM=ON -DUSE_LIGHTGBM=ON
echo.

cd ..
pause
