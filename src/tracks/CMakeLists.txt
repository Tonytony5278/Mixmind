# Track System Library (Instrument Tracks with MIDI → Audio)
add_library(track_system STATIC
    TrackTypes.h
    InstrumentTrack.h
    InstrumentTrack.cpp
)

target_include_directories(track_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(track_system PUBLIC cxx_std_20)

# Link to dependencies
target_link_libraries(track_system PRIVATE 
    core_utilities
    midi_processing
    vsti_hosting
    real_vst3_integration
)

if(WIN32)
    target_compile_definitions(track_system PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()