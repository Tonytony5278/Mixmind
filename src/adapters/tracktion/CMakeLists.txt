# Tracktion Engine Adapter Library
add_library(mixmind_te_adapter STATIC
    TEAdapter.cpp
    TESession.cpp
    TETransport.cpp
    TETrack.cpp
    TEClip.cpp
    TEPlugin.cpp
    TEAutomation.cpp
    TERenderService.cpp
    TEUtils.cpp
    TEVSTScanner.cpp
    TEPluginAdapter.cpp
    
    # Headers
    TEAdapter.h
    TESession.h
    TETransport.h
    TETrack.h
    TEClip.h
    TEPlugin.h
    TEAutomation.h
    TERenderService.h
    TEUtils.h
    TEVSTScanner.h
    TEPluginAdapter.h
)

target_link_libraries(mixmind_te_adapter PUBLIC
    mixmind_core
    tracktion_engine
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_events
    
    # VST3 SDK
    sdk
    sdk_hosting
    sdk_common
)

target_include_directories(mixmind_te_adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_compile_features(mixmind_te_adapter PUBLIC cxx_std_20)

# Link to main application
target_link_libraries(MixMindAI PRIVATE mixmind_te_adapter)