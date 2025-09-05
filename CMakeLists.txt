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

# Set policy for newer CMake versions
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.5")
    cmake_policy(SET CMP0057 NEW) # Support IN_LIST operator
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
    cmake_policy(SET CMP0091 NEW) # Support CMAKE_MSVC_RUNTIME_LIBRARY
endif()

project(MixMindAI VERSION 0.1.0 LANGUAGES CXX C)

# Professional DAW Build Configuration - BUILD REAL CODE, NOT STUB
option(MIXMIND_MINIMAL "Build minimal version without heavy dependencies" OFF)
option(MIXMIND_LEVEL_AUDIO "Build with audio processing capabilities" ON)
option(MIXMIND_ENABLE_GUI "Enable Dear ImGui interface" ON)
option(MIXMIND_ENABLE_AI "Enable AI features" ON)
option(MIXMIND_TRACKTION_ENABLED "Enable Tracktion Engine integration" ON)
option(MIXMIND_JUCE_ENABLED "Enable JUCE framework" ON)
option(BUILD_TESTS "Build test suite" OFF)

# Include FetchContent for dependencies
include(FetchContent)

# ============================================================================
# CRITICAL: Tracktion Engine v3.2.0 - Stable Release (Not develop!)
# ============================================================================

if(MIXMIND_TRACKTION_ENABLED)
    message(STATUS "Setting up Tracktion Engine v3.2.0 (stable release)")
    
    # Tracktion needs JUCE first
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.9
        GIT_PROGRESS TRUE
    )
    
    FetchContent_Declare(
        tracktion_engine
        GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
        GIT_TAG v3.2.0  # Use stable release, not develop branch!
        GIT_PROGRESS TRUE
    )
    
    # Configure JUCE options
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Build JUCE examples")
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Build JUCE extras")
    
    FetchContent_MakeAvailable(juce)
    
    # Configure Tracktion Engine options
    set(TRACKTION_BUILD_EXAMPLES OFF CACHE BOOL "Build Tracktion examples")
    set(TRACKTION_BUILD_TESTS OFF CACHE BOOL "Build Tracktion tests")
    
    FetchContent_MakeAvailable(tracktion_engine)
    
    add_compile_definitions(MIXMIND_TRACKTION_ENABLED=1)
    add_compile_definitions(MIXMIND_JUCE_ENABLED=1)
    
    message(STATUS "Tracktion Engine v3.2.0 configured successfully")
endif()

# Essential Dependencies
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(httplib)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")

# Build options
option(RUBBERBAND_ENABLED "Enable Rubber Band time stretching (commercial)" OFF)
option(BUILD_TESTS "Build unit tests" ON)
option(MIXMIND_MINIMAL "Build minimal, CI-friendly targets (no heavy audio deps)" OFF)

# Incremental build level options - ENABLE FULL FEATURES
option(MIXMIND_LEVEL_AUDIO "Build with audio processing services" ON)
option(MIXMIND_LEVEL_VST "Build with VST3 support" ON)
option(MIXMIND_LEVEL_FULL "Build with all features" ON)

# Create build level logic
if(MIXMIND_LEVEL_FULL)
    set(MIXMIND_LEVEL_VST ON)
endif()

if(MIXMIND_LEVEL_VST)
    set(MIXMIND_LEVEL_AUDIO ON)
endif()

if(MIXMIND_LEVEL_AUDIO)
    set(MIXMIND_MINIMAL OFF)
endif()

# Include FetchContent for dependency management
include(FetchContent)
include(ExternalProject)

# Set up dependency directory
set(DEPS_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps)

###########################################
# Core Dependencies
###########################################

# Essential dependencies (always loaded unless minimal)
if(NOT MIXMIND_MINIMAL)
    # JSON (lightweight, always include)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/nlohmann_json
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# UI Framework Dependencies (ImGui + GLFW)
if(NOT MIXMIND_MINIMAL AND MIXMIND_ENABLE_GUI)
    # Dear ImGui for professional UI
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.0
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/imgui
    )
    FetchContent_MakeAvailable(imgui)
    
    # GLFW for windowing
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.9
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/glfw
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW docs")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples")
    FetchContent_MakeAvailable(glfw)
    
    # Create ImGui library target
    add_library(imgui_lib STATIC
        ${DEPS_DIR}/imgui/imgui.cpp
        ${DEPS_DIR}/imgui/imgui_demo.cpp
        ${DEPS_DIR}/imgui/imgui_draw.cpp
        ${DEPS_DIR}/imgui/imgui_tables.cpp
        ${DEPS_DIR}/imgui/imgui_widgets.cpp
        ${DEPS_DIR}/imgui/backends/imgui_impl_glfw.cpp
        ${DEPS_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_lib PUBLIC 
        ${DEPS_DIR}/imgui
        ${DEPS_DIR}/imgui/backends
    )
    target_link_libraries(imgui_lib PUBLIC glfw)
    
    message(STATUS "✅ ImGui and GLFW UI framework configured")
endif()

# Audio processing dependencies (only for audio level+)
if(MIXMIND_LEVEL_AUDIO)
    # libebur128 for LUFS metering
    FetchContent_Declare(
        libebur128
        GIT_REPOSITORY https://github.com/jiixyj/libebur128.git
        GIT_TAG v1.2.6
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/libebur128
    )
    FetchContent_MakeAvailable(libebur128)
    
    # SoundTouch for time stretching
    FetchContent_Declare(
        soundtouch
        GIT_REPOSITORY https://github.com/stenzek/soundtouch.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/soundtouch
    )
    FetchContent_MakeAvailable(soundtouch)

    # TagLib for metadata
    FetchContent_Declare(
        taglib
        GIT_REPOSITORY https://github.com/taglib/taglib.git
        GIT_TAG v2.0
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/taglib
    )
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    FetchContent_MakeAvailable(taglib)

    # KissFFT for spectrum analysis  
    FetchContent_Declare(
        kissfft
        GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/kissfft
    )
    
    # Configure KissFFT to avoid pkg-config dependency issues
    set(KISSFFT_TOOLS OFF CACHE BOOL "Build KissFFT tools")
    set(KISSFFT_TEST OFF CACHE BOOL "Build KissFFT tests") 
    set(KISSFFT_STATIC ON CACHE BOOL "Build KissFFT as static library")
    FetchContent_MakeAvailable(kissfft)
    
    # cpp-httplib for HTTP requests (OpenAI API)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.14.0
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/httplib
    )
    set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF)
    set(HTTPLIB_COMPILE ON)
    FetchContent_MakeAvailable(httplib)
    
    # Add PortAudio for real-time audio I/O
    FetchContent_Declare(
        portaudio
        GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
        GIT_TAG v19.7.0
        SOURCE_DIR ${DEPS_DIR}/portaudio
    )
    FetchContent_MakeAvailable(portaudio)
    
    # Crow framework for REST and WebSocket servers
    FetchContent_Declare(
        crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG v1.0+5
        GIT_SHALLOW TRUE
        SOURCE_DIR ${DEPS_DIR}/crow
    )
    
    # Configure Crow options
    set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "Build Crow examples")
    set(CROW_BUILD_TESTS OFF CACHE BOOL "Build Crow tests")
    FetchContent_MakeAvailable(crow)
    
    message(STATUS "✅ Crow framework configured for REST/WebSocket servers")
endif()

# VST and heavy audio dependencies (only for VST level+)
if(MIXMIND_LEVEL_VST)
    # Enable advanced plugin hosting for VST+ levels
    set(MIXMIND_ENABLE_PLUGIN_HOSTING ON CACHE BOOL "Enable plugin hosting" FORCE)
    set(MIXMIND_ENABLE_JUCE ON CACHE BOOL "Enable JUCE framework" FORCE)
    
    # Ensure JUCE is available (required by Tracktion)
    if(NOT TARGET juce AND NOT MIXMIND_TRACKTION_ENABLED)
        FetchContent_Declare(
            juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 7.0.9
            GIT_SHALLOW TRUE
            SOURCE_DIR ${DEPS_DIR}/juce
        )
        
        # Configure JUCE options
        set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Build JUCE examples")
        set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Build JUCE extras")
        
        FetchContent_MakeAvailable(juce)
        message(STATUS "✅ JUCE framework configured for VST level")
    endif()
    
    # Tracktion Engine (stable release with fallback)
    option(LOCAL_TRACKTION "Use local Tracktion installation" OFF)
    
    if(LOCAL_TRACKTION)
        message(STATUS "🔧 Using local Tracktion Engine installation")
        find_package(TracktionEngine QUIET)
        if(TracktionEngine_FOUND)
            set(TRACKTION_ENABLED ON)
        else()
            message(WARNING "⚠️ Local Tracktion not found, falling back to download")
            set(LOCAL_TRACKTION OFF)
        endif()
    endif()
    
    if(NOT LOCAL_TRACKTION)
        FetchContent_Declare(
            tracktion_engine
            GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
            GIT_TAG v3.2.0
            GIT_SHALLOW TRUE
            SOURCE_DIR ${DEPS_DIR}/tracktion_engine
        )
    endif()

    # Configure Tracktion options BEFORE MakeAvailable
    set(TRACKTION_BUILD_EXAMPLES OFF CACHE BOOL "Build Tracktion examples")
    set(TRACKTION_BUILD_TESTS OFF CACHE BOOL "Build Tracktion tests") 
    set(TRACKTION_ENABLE_WEBVIEW OFF CACHE BOOL "Enable Tracktion webview")
    set(TRACKTION_ENABLE_ABLETON_LINK OFF CACHE BOOL "Enable Ableton Link")
    
    # Make Tracktion available with error handling
    message(STATUS "Attempting to fetch Tracktion Engine...")
    FetchContent_GetProperties(tracktion_engine)
    if(NOT tracktion_engine_POPULATED)
        message(STATUS "Populating Tracktion Engine...")
        FetchContent_Populate(tracktion_engine)
        
        # Only add subdirectory if CMakeLists.txt exists
        if(EXISTS "${tracktion_engine_SOURCE_DIR}/CMakeLists.txt")
            add_subdirectory(${tracktion_engine_SOURCE_DIR} ${tracktion_engine_BINARY_DIR} EXCLUDE_FROM_ALL)
            set(TRACKTION_ENABLED ON)
            message(STATUS "✅ Tracktion Engine successfully integrated")
        else()
            message(WARNING "⚠️  Tracktion Engine source incomplete - VST features disabled")
            set(TRACKTION_ENABLED OFF)
        endif()
    endif()
    
    # VST3 SDK (only if Tracktion is available)
    if(TRACKTION_ENABLED)
        FetchContent_Declare(
            vst3sdk
            GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
            GIT_TAG v3.7.7_build_19
            GIT_SHALLOW TRUE
            SOURCE_DIR ${DEPS_DIR}/vst3sdk
        )

        set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "Enable VST3 plugin examples")
        set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "Enable VST3 hosting examples") 
        set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "Create plugin link")
        
        message(STATUS "Attempting to fetch VST3 SDK...")
        FetchContent_MakeAvailable(vst3sdk)
        message(STATUS "✅ VST3 SDK integrated successfully")
    else()
        message(STATUS "🚫 Skipping VST3 SDK - Tracktion not available")
    endif()
endif()

# Time stretching libraries configured above in the full build section

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
    add_compile_options($<$<CONFIG:Debug>:/Od>)
    add_compile_options($<$<CONFIG:Debug>:/Zi>)
    add_link_options($<$<CONFIG:Debug>:/DEBUG>)
    
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

# Conditional source sets based on build mode
if(MIXMIND_MINIMAL)
  # Ultra-minimal stub for CI success (no dependencies)
  set(MIXMIND_SOURCES_CORE
    src/main_minimal.cpp
  )
else()
  # COMPREHENSIVE MixMind AI sources - EVERY IMPLEMENTED FEATURE
  set(MIXMIND_SOURCES
    # Main application
    src/main.cpp
    src/MixMindApp.cpp
    src/MixMindApp.h
    
    # Core systems
    src/core/async.cpp
    src/core/async.h
    src/core/result.h
    src/core/types.h
    
    # Audio Engine - ALL files
    src/audio/AudioEngine.cpp
    src/audio/AudioEngine.h
    src/audio/RealtimeAudioEngine.cpp
    src/audio/RealtimeAudioEngine.h
    src/audio/LockFreeBuffer.cpp
    src/audio/LockFreeBuffer.h
    src/audio/LUFSNormalizer.cpp
    src/audio/LUFSNormalizer.h
    src/audio/MeterProcessor.cpp
    src/audio/MeterProcessor.h
    src/audio/WAVWriter.cpp
    src/audio/WAVWriter.h
    src/audio/generators/AudioGenerator.cpp
    src/audio/generators/AudioGenerator.h
    
    # Complete AI system
    src/ai/AIAssistant.cpp
    src/ai/AIAssistant.h
    src/ai/ChatService.cpp
    src/ai/ChatService.h
    src/ai/IntelligentProcessor.cpp
    src/ai/IntelligentProcessor.h
    src/ai/IntentRecognition.cpp
    src/ai/IntentRecognition.h
    src/ai/MusicGenerator.cpp
    src/ai/MusicGenerator.h
    src/ai/MusicKnowledgeBase.cpp
    src/ai/MusicKnowledgeBase.h
    src/ai/OpenAIIntegration.cpp
    src/ai/OpenAIIntegration.h
    src/ai/PhraseMappingService.cpp
    src/ai/PhraseMappingService.h
    src/ai/ProactiveMonitor.cpp
    src/ai/ProactiveMonitor.h
    src/ai/PreferenceLearning.cpp
    src/ai/PreferenceLearning.h
    src/ai/StyleMatcher.cpp
    src/ai/StyleMatcher.h
    src/ai/StyleTransfer.cpp
    src/ai/StyleTransfer.h
    src/ai/VoiceControl.cpp
    src/ai/VoiceControl.h
    
    # Tracktion adapters - ALL files
    src/adapters/tracktion/TEAdapter.cpp
    src/adapters/tracktion/TEAdapter.h
    src/adapters/tracktion/TEAutomation.cpp
    src/adapters/tracktion/TEAutomation.h
    src/adapters/tracktion/TEClip.cpp
    src/adapters/tracktion/TEClip.h
    src/adapters/tracktion/TEPlugin.cpp
    src/adapters/tracktion/TEPlugin.h
    src/adapters/tracktion/TEPluginAdapter.cpp
    src/adapters/tracktion/TEPluginAdapter.h
    src/adapters/tracktion/TERenderService.cpp
    src/adapters/tracktion/TERenderService.h
    src/adapters/tracktion/TESession.cpp
    src/adapters/tracktion/TESession.h
    src/adapters/tracktion/TETrack.cpp
    src/adapters/tracktion/TETrack.h
    src/adapters/tracktion/TETransport.cpp
    src/adapters/tracktion/TETransport.h
    src/adapters/tracktion/TEUtils.cpp
    src/adapters/tracktion/TEUtils.h
    src/adapters/tracktion/TEVSTScanner.cpp
    src/adapters/tracktion/TEVSTScanner.h
    src/engine/TracktionDAW.cpp
    src/engine/TracktionDAW.h
    
    # Services - ALL implementations
    src/services/KissFFTService.cpp
    src/services/KissFFTService.h
    src/services/LibEBU128Service.cpp
    src/services/LibEBU128Service.h
    src/services/OSCService.cpp
    src/services/OSCService.h
    src/services/RealOpenAIService.cpp
    src/services/RealOpenAIService.h
    src/services/SpeechRecognitionService.cpp
    src/services/SpeechRecognitionService.h
    src/services/TagLibService.cpp
    src/services/TagLibService.h
    src/services/TimeStretchService.cpp
    src/services/TimeStretchService.h
    
    # API servers
    src/api/RESTServer.cpp
    src/api/RESTServer.h
    src/api/WebSocketServer.cpp
    src/api/WebSocketServer.h
    
    # UI - ALL components
    src/ui/AIChatWidget.cpp
    src/ui/AIChatWidget.h
    src/ui/CCLaneEditor.cpp
    src/ui/CCLaneEditor.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/PianoRollEditor.cpp
    src/ui/PianoRollEditor.h
    src/ui/StepSequencer.cpp
    src/ui/StepSequencer.h
    src/ui/Theme.cpp
    src/ui/Theme.h
    src/ui/TransportBar.cpp
    src/ui/TransportBar.h
    
    # Plugin system - UNIVERSAL BRIDGE
    src/plugins/PluginHost.cpp
    src/plugins/PluginHost.h
    src/plugins/PluginIntelligence.cpp
    src/plugins/PluginIntelligence.h
    src/plugins/RealVST3Plugin.cpp
    src/plugins/RealVST3Plugin.h
    src/plugins/VST3Plugin.cpp
    src/plugins/VST3Plugin.h
    src/plugins/UniversalPluginBridge.cpp
    src/plugins/UniversalPluginBridge.h
    
    # VST3 integration
    src/vst3/RealVST3Scanner.cpp
    src/vst3/RealVST3Scanner.h
    src/vsti/VSTiHost.cpp
    src/vsti/VSTiHost.h
    
    # MIDI processing
    src/midi/MIDIClip.cpp
    src/midi/MIDIClip.h
    src/midi/MIDIProcessor.cpp
    src/midi/MIDIProcessor.h
    
    # Audio mixing
    src/mixer/AudioBus.cpp
    src/mixer/AudioBus.h
    
    # Parameter automation
    src/automation/AutomationData.cpp
    src/automation/AutomationData.h
    src/automation/AutomationEditor.cpp
    src/automation/AutomationEditor.h
    src/automation/AutomationEngine.cpp
    src/automation/AutomationEngine.h
    src/automation/AutomationRecorder.cpp
    src/automation/AutomationRecorder.h
    src/automation/ParameterAutomation.cpp
    src/automation/ParameterAutomation.h
    
    # Track management
    src/tracks/InstrumentTrack.cpp
    src/tracks/InstrumentTrack.h
    
    # Rendering system
    src/render/AudioFileWriter.cpp
    src/render/AudioFileWriter.h
    src/render/RenderEngine.cpp
    src/render/RenderEngine.h
    
    # Performance monitoring
    src/performance/PerformanceMonitor.cpp
    src/performance/PerformanceMonitor.h
    
    # DSP optimization
    src/dsp/SIMDProcessor.cpp
    src/dsp/SIMDProcessor.h
  )
endif()

# Create main executable with comprehensive source list
if(MIXMIND_MINIMAL)
  add_executable(MixMindAI ${MIXMIND_SOURCES_CORE})
else()
  add_executable(MixMindAI ${MIXMIND_SOURCES})
endif()
target_compile_definitions(MixMindAI PRIVATE MIXMIND_MINIMAL=$<BOOL:${MIXMIND_MINIMAL}>)

# Add subdirectories based on build level
if(NOT MIXMIND_MINIMAL)
    # Add src directory which includes services
    add_subdirectory(src)
    
    # Include benchmark suite for performance validation
    add_subdirectory(benchmarks)
    
    # Only include heavy subdirectories for VST+ builds
    if(ENABLE_VST3_HOSTING OR ENABLE_TRACKTION_ENGINE)
        add_subdirectory(apps/mixmind_app)
        add_subdirectory(src/demo)
    endif()
endif()

# Link libraries to main executable (conditional)
if(NOT MIXMIND_MINIMAL)
    target_link_libraries(MixMindAI PRIVATE
        # JSON processing
        nlohmann_json::nlohmann_json
    )
endif()

# Link heavy audio dependencies only when not minimal
if(NOT MIXMIND_MINIMAL)
    target_link_libraries(MixMindAI PRIVATE
        # Audio processing libraries (FIXED NAMES)
        ebur128                 # libebur128 target name
        SoundTouch              # SoundTouch proper target name
        tag                     # TagLib target name
        kissfft                 # KissFFT target name
        httplib                 # cpp-httplib target name
        portaudio               # PortAudio for real-time audio I/O
        Crow::Crow              # Crow framework for REST/WebSocket servers
        
        # Tracktion Engine (conditional)
        $<$<BOOL:${TRACKTION_ENABLED}>:$<TARGET_NAME_IF_EXISTS:tracktion_engine>>
        
        # VST3 SDK (conditional - only if Tracktion is enabled)
        $<$<BOOL:${TRACKTION_ENABLED}>:$<TARGET_NAME_IF_EXISTS:sdk>>
        $<$<BOOL:${TRACKTION_ENABLED}>:$<TARGET_NAME_IF_EXISTS:vstgui_support>>
        $<$<BOOL:${TRACKTION_ENABLED}>:$<TARGET_NAME_IF_EXISTS:base>>
        
        # Advanced plugin hosting system (conditional)
        $<$<BOOL:${MIXMIND_PLUGIN_HOSTING_ENABLED}>:${MIXMIND_PLUGIN_LIBRARY}>
    )
    
    # Windows-specific libraries for voice control and VST hosting
    if(WIN32)
        target_link_libraries(MixMindAI PRIVATE
            winmm       # Windows Multimedia API for audio recording/playback
            ole32       # COM for VST plugin hosting  
            uuid        # UUID generation for plugin identification
            ws2_32      # Winsock for networking (REST/WebSocket servers)
            opengl32    # OpenGL for Dear ImGui rendering
        )
        message(STATUS "✅ Windows system libraries linked for voice control and VST hosting")
    endif()
    
    # UI Framework libraries (conditional)
    if(MIXMIND_ENABLE_GUI)
        target_link_libraries(MixMindAI PRIVATE
            imgui_lib   # Dear ImGui professional interface
            glfw        # GLFW windowing system
        )
        message(STATUS "✅ UI framework libraries linked")
    endif()
    
    # Set include directories for external libraries
    target_include_directories(MixMindAI PRIVATE
        ${DEPS_DIR}/libebur128/ebur128
        ${DEPS_DIR}/soundtouch/include
        ${DEPS_DIR}/taglib/taglib
        ${DEPS_DIR}/kissfft
        ${DEPS_DIR}/httplib
        ${DEPS_DIR}/portaudio/include
    )
endif()

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
message(STATUS "")
message(STATUS "Plugin Hosting:")
message(STATUS "  Plugin Hosting: ${MIXMIND_PLUGIN_HOSTING_ENABLED}")
message(STATUS "  JUCE Framework: ${JUCE_AVAILABLE}")
message(STATUS "  VST3 Support: ${MIXMIND_SUPPORT_VST3}")
message(STATUS "  Audio Units: ${MIXMIND_SUPPORT_AU}")
message(STATUS "  CLAP Support: ${MIXMIND_SUPPORT_CLAP}")
message(STATUS "  AI Analysis: ${MIXMIND_PLUGIN_AI_ANALYSIS}")
message(STATUS "=====================================")
message(STATUS "")