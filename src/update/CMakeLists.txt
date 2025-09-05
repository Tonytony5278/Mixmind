# MixMind AI - Auto-Update System
# Seamless update delivery with digital signature verification

add_library(MixMindAutoUpdater
    AutoUpdater.cpp
    AutoUpdater.h
)

target_link_libraries(MixMindAutoUpdater 
    PRIVATE
    MixMindCore
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Windows-specific libraries for installer execution
if(WIN32)
    target_link_libraries(MixMindAutoUpdater PRIVATE
        shell32
    )
endif()

target_include_directories(MixMindAutoUpdater 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(MixMindAutoUpdater PRIVATE cxx_std_20)

# Add to main target
target_link_libraries(MixMindAI PRIVATE MixMindAutoUpdater)