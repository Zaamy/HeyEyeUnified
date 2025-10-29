cmake_minimum_required(VERSION 3.15)

# Set policy for Boost (suppress CMP0167 warning)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

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
    ${SRC_DIR}/lightgbm_ranker.cpp
    ${SRC_DIR}/ranking_features.cpp
    ${SRC_DIR}/ranking_features_helper.cpp
    ${SRC_DIR}/ml_helpers.cpp
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
    ${INCLUDE_DIR}/lightgbm_ranker.h
    ${INCLUDE_DIR}/ranking_features.h
    ${INCLUDE_DIR}/ml_helpers.h
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

        # Copy ONNX Runtime DLLs to output directory on Windows
        if(WIN32)
            # Get the directory containing the ONNX Runtime library
            get_filename_component(ONNXRUNTIME_LIB_DIR ${ONNXRUNTIME_LIBRARY} DIRECTORY)

            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>
                COMMENT "Copying onnxruntime.dll to output directory"
            )

            # Also copy providers shared library if it exists
            if(EXISTS "${ONNXRUNTIME_LIB_DIR}/onnxruntime_providers_shared.dll")
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${ONNXRUNTIME_LIB_DIR}/onnxruntime_providers_shared.dll"
                        $<TARGET_FILE_DIR:${PROJECT_NAME}>
                    COMMENT "Copying onnxruntime_providers_shared.dll to output directory"
                )
            endif()
        endif()
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

    # Find Boost libraries (required by KenLM)
    # Note: KenLM primarily needs system, filesystem, and thread
    find_package(Boost COMPONENTS system filesystem thread REQUIRED)

    if(KENLM_INCLUDE_DIR AND KENLM_LIBRARY)
        target_include_directories(${PROJECT_NAME} PRIVATE ${KENLM_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${KENLM_LIBRARY})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_KENLM)
        target_compile_definitions(${PROJECT_NAME} PRIVATE KENLM_MAX_ORDER=6)
        message(STATUS "KenLM found: ${KENLM_LIBRARY}")

        # Link Boost libraries
        if(Boost_FOUND)
            target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE
                Boost::system
                Boost::filesystem
                Boost::thread
            )
            message(STATUS "Boost found: ${Boost_VERSION}")
            message(STATUS "  Boost components: system, filesystem, thread")
        else()
            message(FATAL_ERROR "Boost not found - required for KenLM")
        endif()
    else()
        message(WARNING "KenLM not found")
        message(STATUS "  KENLM_INCLUDE_DIR: ${KENLM_INCLUDE_DIR}")
        message(STATUS "  KENLM_LIBRARY: ${KENLM_LIBRARY}")
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

# msgpack-c (header-only library for vocabulary loading)
option(USE_MSGPACK "Enable msgpack for vocabulary loading" OFF)
if(USE_MSGPACK)
    find_path(MSGPACK_INCLUDE_DIR msgpack.hpp
        PATHS
            D:/Deps/vcpkg/installed/x64-windows/include
            /usr/local/include
            /usr/include
    )
    if(MSGPACK_INCLUDE_DIR)
        target_include_directories(${PROJECT_NAME} PRIVATE ${MSGPACK_INCLUDE_DIR})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_MSGPACK)
        message(STATUS "msgpack-c found: ${MSGPACK_INCLUDE_DIR}")
    else()
        message(WARNING "msgpack-c not found")
    endif()
endif()

# Tobii Stream Engine SDK
option(USE_TOBII "Enable Tobii eye tracking" ON)
if(USE_TOBII)
    # Use local tobii directory in HeyEyeUnified project
    # find_path looks for the parent dir that contains tobii/tobii.h
    find_path(TOBII_INCLUDE_DIR tobii/tobii.h
        PATHS
            ${PROJECT_SOURCE_DIR}
        NO_DEFAULT_PATH
    )
    # find_library looks for the actual .lib file in tobii/
    find_library(TOBII_LIBRARY
        NAMES tobii_stream_engine
        PATHS
            ${PROJECT_SOURCE_DIR}/tobii
        NO_DEFAULT_PATH
    )
    if(TOBII_INCLUDE_DIR AND TOBII_LIBRARY)
        target_include_directories(${PROJECT_NAME} PRIVATE ${TOBII_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${TOBII_LIBRARY})
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_TOBII)
        message(STATUS "Tobii SDK found at: ${TOBII_INCLUDE_DIR}")
        message(STATUS "Tobii library: ${TOBII_LIBRARY}")

        # Copy DLL to output directory on Windows
        if(WIN32)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PROJECT_SOURCE_DIR}/tobii/tobii_stream_engine.dll"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>
                COMMENT "Copying Tobii DLL to output directory"
            )
        endif()
    else()
        message(WARNING "Tobii SDK not found - running in manual mode")
        message(STATUS "  Searched include path: ${PROJECT_SOURCE_DIR}/tobii")
        message(STATUS "  TOBII_INCLUDE_DIR: ${TOBII_INCLUDE_DIR}")
        message(STATUS "  TOBII_LIBRARY: ${TOBII_LIBRARY}")
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

    # Enable Link-Time Code Generation for Release builds (matches KenLM library)
    if(USE_KENLM)
        target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/GL>)
        target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/LTCG>)
    endif()
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
message(STATUS "  msgpack-c:    ${USE_MSGPACK}")
message(STATUS "  Tobii SDK:    ${USE_TOBII}")
message(STATUS "")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "===================================")
message(STATUS "")
