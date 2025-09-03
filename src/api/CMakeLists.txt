# API Services Library
add_library(mixmind_api STATIC
    ActionAPI.cpp
    RESTServer.cpp
    WebSocketServer.cpp
    
    # Headers
    ActionAPI.h
    ActionSchemas.h
    RESTServer.h
    WebSocketServer.h
)

target_link_libraries(mixmind_api PUBLIC
    mixmind_core
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator
    httplib::httplib
    websocketpp::websocketpp
)

target_include_directories(mixmind_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(mixmind_api PUBLIC cxx_std_20)

# Link to main application
target_link_libraries(MixMindAI PRIVATE mixmind_api)