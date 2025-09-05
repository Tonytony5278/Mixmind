# MixMind AI - Analytics & Telemetry System
# Professional usage tracking with privacy compliance

add_library(MixMindAnalytics
    Analytics.cpp
    Analytics.h
)

target_link_libraries(MixMindAnalytics 
    PRIVATE
    MixMindCore
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Windows-specific libraries for performance monitoring
if(WIN32)
    target_link_libraries(MixMindAnalytics PRIVATE
        pdh
        psapi
    )
endif()

target_include_directories(MixMindAnalytics 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(MixMindAnalytics PRIVATE cxx_std_20)

# Add to main target
target_link_libraries(MixMindAI PRIVATE MixMindAnalytics)