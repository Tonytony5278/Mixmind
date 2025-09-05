# Source code organization (expanded for Alpha VSTi hosting)
add_subdirectory(core)
add_subdirectory(adapters)
add_subdirectory(services)
add_subdirectory(midi)        # MIDI processing for instrument tracks

# Production systems (always included)
add_subdirectory(licensing)   # Commercial license management
add_subdirectory(analytics)   # Usage tracking and telemetry
add_subdirectory(update)      # Auto-update system

# Heavy audio dependencies (VST3, VSTI, AI) - only when not minimal
if(NOT MIXMIND_MINIMAL)
    add_subdirectory(vsti)        # VST instrument hosting
    add_subdirectory(tracks)      # Instrument track implementation  
    add_subdirectory(vst3)        # Real VST3 integration
    add_subdirectory(ai)          # AI functionality with heavy dependencies
endif()
# add_subdirectory(api)      # Temporarily disabled for VST3 proof
# add_subdirectory(ui)       # Temporarily disabled for VST3 proof

# E2E Tests for VST3 proof (only when not minimal)
if(NOT MIXMIND_MINIMAL)
    add_subdirectory(../tests/e2e ${CMAKE_CURRENT_BINARY_DIR}/e2e_tests)
endif()

# Main application sources
target_sources(MixMindAI PRIVATE
    main.cpp
    MixMindApp.cpp
    MixMindApp.h
)