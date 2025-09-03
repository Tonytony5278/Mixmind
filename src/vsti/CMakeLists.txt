# VST Instrument Hosting Library
add_library(vsti_hosting STATIC
    VSTiHost.h
    VSTiHost.cpp
)

target_include_directories(vsti_hosting PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(vsti_hosting PUBLIC cxx_std_20)

# Link to dependencies
target_link_libraries(vsti_hosting PRIVATE 
    core_utilities
    real_vst3_integration
    midi_processing
)

if(WIN32)
    target_compile_definitions(vsti_hosting PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()