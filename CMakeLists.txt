# CMake version requirement with friendly error message
if(CMAKE_VERSION VERSION_LESS "3.22")
    message(FATAL_ERROR "
=== CMake Version Too Old ===
Current version: ${CMAKE_VERSION}
Required version: 3.22 or higher

To upgrade CMake:

Windows (Chocolatey):
  choco install cmake --version=3.28.1

Windows (Direct):
  1. Download from: https://cmake.org/download/
  2. Install latest MSI package
  3. Add to PATH or use CMake GUI

macOS (Homebrew):
  brew install cmake

macOS (Direct):
  1. Download from: https://cmake.org/download/
  2. Install DMG package
  
Then retry: cmake -S . -B build
")
endif()

cmake_minimum_required(VERSION 3.22)
project(MixMindAI VERSION 0.1.0 LANGUAGES CXX C)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")

# Build options
option(RUBBERBAND_ENABLED "Enable Rubber Band time stretching (commercial)" OFF)
option(BUILD_TESTS "Build unit tests" ON)

# Include FetchContent for dependency management
include(FetchContent)
include(ExternalProject)

# Set up dependency directory
set(DEPS_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps)

###########################################
# Core Dependencies
###########################################

# Tracktion Engine (includes JUCE) - this is our primary DAW engine
FetchContent_Declare(
    tracktion_engine
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG develop
    SOURCE_DIR ${DEPS_DIR}/tracktion_engine
)

# Configure Tracktion options BEFORE MakeAvailable
set(TRACKTION_BUILD_EXAMPLES OFF CACHE BOOL "Build Tracktion examples")
set(TRACKTION_BUILD_TESTS OFF CACHE BOOL "Build Tracktion tests") 
set(TRACKTION_ENABLE_WEBVIEW OFF CACHE BOOL "Enable Tracktion webview")
set(TRACKTION_ENABLE_ABLETON_LINK OFF CACHE BOOL "Enable Ableton Link")

FetchContent_MakeAvailable(tracktion_engine)

# VST3 SDK with override-able tag  
set(VST3SDK_GIT_TAG "master" CACHE STRING "VST3 SDK tag")
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG ${VST3SDK_GIT_TAG}
    SOURCE_DIR ${DEPS_DIR}/vst3sdk
)

set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "Enable VST3 plugin examples")
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "Enable VST3 hosting examples") 
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "Create plugin link")
FetchContent_MakeAvailable(vst3sdk)

# Essential JSON support
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    SOURCE_DIR ${DEPS_DIR}/nlohmann_json
)
FetchContent_MakeAvailable(nlohmann_json)

# HTTP library for AI integration
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
    SOURCE_DIR ${DEPS_DIR}/httplib
)
set(HTTPLIB_REQUIRE_OPENSSL OFF)
FetchContent_MakeAvailable(httplib)

# Time stretching libraries
# SoundTouch (temporarily disabled for PoC)
# FetchContent_Declare(
#     soundtouch
#     GIT_REPOSITORY https://gitlab.com/soundtouch/soundtouch.git
#     GIT_TAG 2.3.2
#     SOURCE_DIR ${DEPS_DIR}/soundtouch
# )
# FetchContent_MakeAvailable(soundtouch)

# Rubber Band (commercial, optional)
if(RUBBERBAND_ENABLED)
    find_package(RubberBand REQUIRED)
    if(NOT RubberBand_FOUND)
        message(WARNING "Rubber Band not found, falling back to SoundTouch")
        set(RUBBERBAND_ENABLED OFF)
    endif()
endif()

###########################################
# Testing Framework
###########################################

if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        SOURCE_DIR ${DEPS_DIR}/googletest
    )
    
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    enable_testing()
    include(GoogleTest)
endif()

###########################################
# Compiler Configuration
###########################################

if(MSVC)
    add_compile_options(
        /W4          # Warning level 4
        /permissive- # Strict conformance
        /Zc:__cplusplus # Correct __cplusplus macro
        /MP          # Multi-processor compilation
    )
    
    # Debug information
    add_compile_options($<$<CONFIG:Debug>:/Od /Zi /DEBUG>)
    
    # Platform-specific settings
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

###########################################
# Source Code Organization
###########################################

# Create main executable
add_executable(MixMindAI)

# Add subdirectories for source code
add_subdirectory(src)

# Link essential libraries to main executable
target_link_libraries(MixMindAI PRIVATE
    # JUCE core
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_events
    
    # Tracktion Engine
    tracktion_engine
    
    # VST3 SDK
    sdk
    sdk_hosting
    sdk_common
    
    # JSON processing
    nlohmann_json::nlohmann_json
)

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Print configuration summary
message(STATUS "")
message(STATUS "=== MixMind AI Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "VST3 SDK tag: ${VST3SDK_GIT_TAG}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  Rubber Band: ${RUBBERBAND_ENABLED}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  Tracktion Examples: ${TRACKTION_BUILD_EXAMPLES}")
message(STATUS "  Tracktion Tests: ${TRACKTION_BUILD_TESTS}")
message(STATUS "=====================================")
message(STATUS "")