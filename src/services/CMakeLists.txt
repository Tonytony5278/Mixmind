# OSS Services Library
add_library(mixmind_services STATIC
    KissFFTService.cpp
    LibEBU128Service.cpp
    OSCService.cpp
    TagLibService.cpp
    TimeStretchService.cpp
    
    # Headers
    KissFFTService.h
    LibEBU128Service.h
    OSCService.h
    TagLibService.h
    IOSSService.h
    ONNXService.h
    OSSServiceRegistry.h
    TimeStretchService.h
)

target_link_libraries(mixmind_services PUBLIC
    mixmind_core
    kissfft
    ebur128
    ${OSC_LIBRARY}
    tag
    
    # Time stretching
    SoundTouch
)

# Optional RubberBand support
if(RUBBERBAND_ENABLED)
    target_link_libraries(mixmind_services PUBLIC RubberBand::RubberBand)
endif()

target_include_directories(mixmind_services PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(mixmind_services PUBLIC cxx_std_20)

# Link to main application
target_link_libraries(MixMindAI PRIVATE mixmind_services)