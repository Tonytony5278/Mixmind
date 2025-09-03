# VST3 End-to-End Tests
# Simplified test (mock implementation)
add_executable(vst3_e2e_test test_vst3_simple.cpp)

# Real VST3 integration test
add_executable(real_vst3_test test_real_vst3.cpp)

# Alpha VSTi hosting test (MIDI → Audio)
add_executable(instrument_hosting_test ../test_instrument_hosting.cpp)

# Common compile features
target_compile_features(vst3_e2e_test PRIVATE cxx_std_20)
target_compile_features(real_vst3_test PRIVATE cxx_std_20)
target_compile_features(instrument_hosting_test PRIVATE cxx_std_20)

# Link GoogleTest
target_link_libraries(real_vst3_test PRIVATE gtest gtest_main)
target_link_libraries(instrument_hosting_test PRIVATE 
    gtest 
    gtest_main
    core_utilities
    midi_processing
    vsti_hosting
    real_vst3_integration
    track_system
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(vst3_e2e_test PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    
    target_compile_definitions(real_vst3_test PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    
    target_compile_definitions(instrument_hosting_test PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(real_vst3_test)
gtest_discover_tests(instrument_hosting_test)