cmake_minimum_required(VERSION 3.15)
project(HeyEyeUnified VERSION 1.0.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find wxWidgets (using modern CMake config from vcpkg)
find_package(wxWidgets CONFIG REQUIRED)
# Note: vcpkg provides wxWidgets via modern CMake config, not FindwxWidgets

# Project directories
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(ASSETS_DIR ${PROJECT_SOURCE_DIR}/assets)

# Source files
set(SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/keybutton.cpp
    ${SRC_DIR}/keyboardview.cpp
    ${SRC_DIR}/gazetracker.cpp
    ${SRC_DIR}/textinputengine.cpp
    ${SRC_DIR}/eyeoverlay.cpp
    ${SRC_DIR}/CircularButton.cpp
    ${SRC_DIR}/settings.cpp
)

# Header files
set(HEADERS
    ${INCLUDE_DIR}/keybutton.h
    ${INCLUDE_DIR}/keyboardview.h
    ${INCLUDE_DIR}/gazetracker.h
    ${INCLUDE_DIR}/textinputengine.h
    ${INCLUDE_DIR}/eyeoverlay.h
    ${INCLUDE_DIR}/CircularButton.h
    ${INCLUDE_DIR}/settings.h
)

# Create executable
if(WIN32)
    # Windows GUI application (no console)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${INCLUDE_DIR}
)

# Link wxWidgets (using modern target-based approach from vcpkg)
target_link_libraries(${PROJECT_NAME} PRIVATE
    wx::core
    wx::base
)

# ============================================================================
# Optional ML Dependencies
# Uncomment sections as needed for full functionality
# ============================================================================

# ONNX Runtime (for swipe encoder model)
option(USE_ONNX "Enable ONNX Runtime for ML swipe prediction" OFF)
if(USE_ONNX)
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
        PATHS
            ${PROJECT_SOURCE_DIR}/../to_combined/HeyEyeTracker/onnxruntime-win-x64-1.19.2/include
            /usr/local/include
            /usr/include
    )
    find_library(ONNXRUNTIME_LIBRARY
        NAMES onnxruntime
        PATHS
            ${PROJECT_SOURCE_DIR}/../to_combined/HeyEyeTracker/onnxruntime-win-x64-1.19.2/lib
            /usr/local/lib
            /usr/lib
    )
    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
        target_include_directories(${PROJECT_NAME} PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${ONNXRUNTIME_LIBRARY})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ONNX)
        message(STATUS "ONNX Runtime found: ${ONNXRUNTIME_LIBRARY}")
    else()
        message(WARNING "ONNX Runtime not found")
    endif()
endif()

# FAISS (for vector similarity search)
option(USE_FAISS "Enable FAISS for vocabulary search" OFF)
if(USE_FAISS)
    find_path(FAISS_INCLUDE_DIR faiss/IndexFlat.h
        PATHS
            D:/Deps/vcpkg/installed/x64-windows/include
            /usr/local/include
            /usr/include
    )
    find_library(FAISS_LIBRARY
        NAMES faiss
        PATHS
            D:/Deps/vcpkg/installed/x64-windows/lib
            /usr/local/lib
            /usr/lib
    )
    if(FAISS_INCLUDE_DIR AND FAISS_LIBRARY)
        target_include_directories(${PROJECT_NAME} PRIVATE ${FAISS_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${FAISS_LIBRARY})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_FAISS)
        message(STATUS "FAISS found: ${FAISS_LIBRARY}")
    else()
        message(WARNING "FAISS not found")
    endif()
endif()

# KenLM (for language model scoring)
option(USE_KENLM "Enable KenLM for language model" OFF)
if(USE_KENLM)
    find_path(KENLM_INCLUDE_DIR lm/model.hh
        PATHS
            D:/Deps/kenlm
            /usr/local/include
            /usr/include
    )
    find_library(KENLM_LIBRARY
        NAMES kenlm_x64 kenlm
        PATHS
            D:/Deps/kenlm/windows
            D:/Deps/kenlm/build
            /usr/local/lib
            /usr/lib
    )
    if(KENLM_INCLUDE_DIR AND KENLM_LIBRARY)
        target_include_directories(${PROJECT_NAME} PRIVATE ${KENLM_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${KENLM_LIBRARY})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_KENLM)
        message(STATUS "KenLM found: ${KENLM_LIBRARY}")
    else()
        message(WARNING "KenLM not found")
    endif()
endif()

# LightGBM (for ranking)
option(USE_LIGHTGBM "Enable LightGBM for ranking" OFF)
if(USE_LIGHTGBM)
    find_path(LIGHTGBM_INCLUDE_DIR LightGBM/c_api.h
        PATHS
            D:/Deps/vcpkg/installed/x64-windows/include
            /usr/local/include
            /usr/include
    )
    find_library(LIGHTGBM_LIBRARY
        NAMES lib_lightgbm _lightgbm
        PATHS
            D:/Deps/vcpkg/installed/x64-windows/lib
            /usr/local/lib
            /usr/lib
    )
    if(LIGHTGBM_INCLUDE_DIR AND LIGHTGBM_LIBRARY)
        target_include_directories(${PROJECT_NAME} PRIVATE ${LIGHTGBM_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${LIGHTGBM_LIBRARY})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LIGHTGBM)
        message(STATUS "LightGBM found: ${LIGHTGBM_LIBRARY}")
    else()
        message(WARNING "LightGBM not found")
    endif()
endif()

# Tobii Stream Engine SDK
option(USE_TOBII "Enable Tobii eye tracking" OFF)
if(USE_TOBII)
    find_path(TOBII_INCLUDE_DIR tobii/tobii.h
        PATHS
            ${PROJECT_SOURCE_DIR}/../to_combined/HeyEyeTracker/tobii
            ${PROJECT_SOURCE_DIR}/../to_combined/HeyEyeControl/tobii
    )
    find_library(TOBII_LIBRARY
        NAMES tobii_stream_engine
        PATHS
            ${PROJECT_SOURCE_DIR}/../to_combined/HeyEyeTracker/tobii
            ${PROJECT_SOURCE_DIR}/../to_combined/HeyEyeControl/tobii
    )
    if(TOBII_INCLUDE_DIR AND TOBII_LIBRARY)
        target_include_directories(${PROJECT_NAME} PRIVATE ${TOBII_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${TOBII_LIBRARY})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_TOBII)
        message(STATUS "Tobii SDK found: ${TOBII_LIBRARY}")
    else()
        message(WARNING "Tobii SDK not found - running in manual mode")
    endif()
endif()

# ============================================================================
# Windows-specific settings
# ============================================================================
if(WIN32)
    # Required for mouse/cursor control
    target_link_libraries(${PROJECT_NAME} PRIVATE user32)

    # Elevated privileges for overlay (optional)
    # Requires signing with certificate
    # set_target_properties(${PROJECT_NAME} PROPERTIES
    #     LINK_FLAGS "/MANIFESTUAC:\"level='asInvoker' uiAccess='true'\""
    # )

    # Enable large object files for ML libraries
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)

    # Increase compiler heap size
    target_compile_options(${PROJECT_NAME} PRIVATE /Zm600)
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Copy assets directory
install(DIRECTORY ${ASSETS_DIR}
    DESTINATION bin
    PATTERN "*.md" EXCLUDE
)

# ============================================================================
# Build output
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Copy assets to build directory for testing
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Creating assets directory in build output"
)

# Copy DLLs on Windows if using shared libraries
if(WIN32)
    # wxWidgets DLLs (if using shared build)
    # ONNX Runtime DLLs
    # Add custom commands as needed
endif()

# ============================================================================
# Status output
# ============================================================================
message(STATUS "")
message(STATUS "=== HeyEye Unified Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "wxWidgets: Found (vcpkg)")
message(STATUS "")
message(STATUS "Optional features:")
message(STATUS "  ONNX Runtime: ${USE_ONNX}")
message(STATUS "  FAISS:        ${USE_FAISS}")
message(STATUS "  KenLM:        ${USE_KENLM}")
message(STATUS "  LightGBM:     ${USE_LIGHTGBM}")
message(STATUS "  Tobii SDK:    ${USE_TOBII}")
message(STATUS "")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "===================================")
message(STATUS "")
