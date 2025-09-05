# MixMind AI - Commercial License Management System
# Professional licensing with hardware fingerprinting and feature gating

add_library(MixMindLicensing
    LicenseManager.cpp
    LicenseManager.h
)

target_link_libraries(MixMindLicensing 
    PRIVATE
    MixMindCore
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Windows-specific libraries for hardware fingerprinting
if(WIN32)
    target_link_libraries(MixMindLicensing PRIVATE
        crypt32
        iphlpapi
        wbemuuid
    )
endif()

target_include_directories(MixMindLicensing 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(MixMindLicensing PRIVATE cxx_std_20)

# Add to main target
target_link_libraries(MixMindAI PRIVATE MixMindLicensing)