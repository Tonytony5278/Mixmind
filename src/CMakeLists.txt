# Source code organization (expanded for Alpha VSTi hosting)
add_subdirectory(core)
add_subdirectory(adapters)
add_subdirectory(services)
add_subdirectory(midi)        # MIDI processing for instrument tracks
add_subdirectory(vsti)        # VST instrument hosting
add_subdirectory(tracks)      # Instrument track implementation
add_subdirectory(vst3)        # Real VST3 integration
# add_subdirectory(api)      # Temporarily disabled for VST3 proof
add_subdirectory(ai)
# add_subdirectory(ui)       # Temporarily disabled for VST3 proof

# E2E Tests for VST3 proof
add_subdirectory(../tests/e2e ${CMAKE_CURRENT_BINARY_DIR}/e2e_tests)

# Main application sources
target_sources(MixMindAI PRIVATE
    main.cpp
    MixMindApp.cpp
    MixMindApp.h
)