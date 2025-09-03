# AI Services Library
add_library(mixmind_ai STATIC
    ChatService.cpp
    ActionAPI.cpp
    IntentRecognition.cpp
    AIAssistantTest.cpp
)

# Header files for AI Assistant MVP
target_sources(mixmind_ai INTERFACE
    ChatService.h
    ActionAPI.h
    IntentRecognition.h
    ConversationContext.h
    OpenAIProvider.h
    AIAssistant.h
    
    # Legacy headers
    ContextualAI.h
    MixingAssistant.h
    VoiceController.h
)

target_link_libraries(mixmind_ai PUBLIC
    mixmind_core
    mixmind_services
    
    # HTTP client for OpenAI API
    httplib::httplib
    
    # JSON processing
    nlohmann_json
)

# Heavy AI dependencies (whisper speech recognition)
if(NOT MIXMIND_MINIMAL)
    target_link_libraries(mixmind_ai PUBLIC whisper)
endif()

if(ONNX_ENABLED)
    target_link_libraries(mixmind_ai PUBLIC onnxruntime)
endif()

# Optional dependencies
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    message(STATUS "httplib not found - OpenAI integration will be limited")
endif()

# Test executable for AI Assistant
add_executable(ai_assistant_test AIAssistantTest.cpp)
target_link_libraries(ai_assistant_test PRIVATE mixmind_ai mixmind_core)
target_compile_definitions(ai_assistant_test PRIVATE AI_ASSISTANT_TEST_MAIN)

target_include_directories(mixmind_ai INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(mixmind_ai INTERFACE cxx_std_20)

# Link to main application
target_link_libraries(MixMindAI PRIVATE mixmind_ai)