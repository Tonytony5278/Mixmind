# MIDI Processing Library
add_library(midi_processing STATIC
    MIDIEvent.h
    MIDIProcessor.h
    MIDIProcessor.cpp
)

target_include_directories(midi_processing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(midi_processing PUBLIC cxx_std_20)

# Link to core utilities
target_link_libraries(midi_processing PRIVATE mixmind_core)

if(WIN32)
    target_compile_definitions(midi_processing PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()