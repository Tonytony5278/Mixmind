# Real VST3 Integration Library
add_library(real_vst3_integration STATIC
    RealVST3Scanner.h
    RealVST3Scanner.cpp
)

target_include_directories(real_vst3_integration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(real_vst3_integration PUBLIC cxx_std_20)

# Link to core utilities
target_link_libraries(real_vst3_integration PRIVATE core_utilities)

if(WIN32)
    target_compile_definitions(real_vst3_integration PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()