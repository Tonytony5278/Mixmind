# Test executable
add_executable(MixMindAI_Tests
    # Core tests
    core/TestAsync.cpp
    core/TestCoreTypes.cpp
    
    # Adapter tests
    adapters/TestTEIntegration.cpp
    
    # API tests
    api/TestActionAPI.cpp
)

# Link against the core library and test dependencies
target_link_libraries(MixMindAI_Tests PRIVATE
    # MixMind components
    mixmind_core
    mixmind_te_adapter
    mixmind_services
    mixmind_api
    mixmind_ai
    mixmind_ui
    
    # Test framework
    gtest_main
    gtest
    gmock
    
    # Core dependencies needed for testing
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_events
    
    # Tracktion Engine
    tracktion_engine
    
    # Other dependencies
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator
    tag
    ebur128
    kissfft
    ${OSC_LIBRARY}
    httplib::httplib
    websocketpp::websocketpp
    whisper
)

# Include directories
target_include_directories(MixMindAI_Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add the test to CTest
include(GoogleTest)
gtest_discover_tests(MixMindAI_Tests)