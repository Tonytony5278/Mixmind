# Services Library - conditional based on minimal build
if(NOT MIXMIND_MINIMAL)
    # Full OSS Services Library (heavy dependencies)
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
else()
    # Minimal Services Library (interface only for CI builds)
    add_library(mixmind_services INTERFACE)

    target_link_libraries(mixmind_services INTERFACE
        mixmind_core
    )

    target_include_directories(mixmind_services INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    target_compile_features(mixmind_services INTERFACE cxx_std_20)

    # Link to main application
    target_link_libraries(MixMindAI PRIVATE mixmind_services)
endif()