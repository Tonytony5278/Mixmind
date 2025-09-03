# UI Services (Header-only for now)
add_library(mixmind_ui INTERFACE)

target_sources(mixmind_ui INTERFACE
    WebUI.h
)

target_link_libraries(mixmind_ui INTERFACE
    mixmind_core
    mixmind_api
)

target_include_directories(mixmind_ui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(mixmind_ui INTERFACE cxx_std_20)

# Link to main application
target_link_libraries(MixMindAI PRIVATE mixmind_ui)