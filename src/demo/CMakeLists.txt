# Real Audio Demo - Standalone executable to prove MixMind AI generates actual sound

add_executable(mixmind_real_audio_demo
    RealAudioDemo.cpp
    ../audio/WAVWriter.cpp
    ../audio/generators/AudioGenerator.cpp
    ../ai/Actions.cpp  # Needed for some generator types
)

target_include_directories(mixmind_real_audio_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(mixmind_real_audio_demo PRIVATE
    mixmind_core
)

target_compile_features(mixmind_real_audio_demo PRIVATE cxx_std_20)

# Set output directory
set_target_properties(mixmind_real_audio_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demo
    OUTPUT_NAME "mixmind_real_audio_demo"
)