# Services Library - conditional based on minimal build
if(NOT MIXMIND_MINIMAL)
    # Full OSS Services Library (heavy dependencies)
    add_library(mixmind_services STATIC
        # KissFFTService.cpp  # Temporarily disabled due to compilation issues
        # LibEBU128Service.cpp  # Temporarily disabled due to compilation issues
        # OSCService.cpp  # Disabled temporarily for build stability
        # TagLibService.cpp  # Temporarily disabled due to compilation issues
        # TimeStretchService.cpp  # Temporarily disabled due to compilation issues
        SpeechRecognitionService.cpp  # This one has fewer dependencies
        
        # Headers
        # KissFFTService.h  # Temporarily disabled
        # LibEBU128Service.h  # Temporarily disabled
        # OSCService.h  # Disabled temporarily
        # TagLibService.h  # Temporarily disabled
        IOSSService.h
        ONNXService.h
        OSSServiceRegistry.h
        SpeechRecognitionService.h
        # TimeStretchService.h  # Temporarily disabled
    )

    target_link_libraries(mixmind_services PUBLIC
        mixmind_core
    )
    
    # Link libraries with correct FetchContent target names
    # Note: FetchContent creates targets with the project name from their CMakeLists.txt
    
    # Temporarily disabled all audio library linking for minimal build
    # These will be re-enabled once the basic build structure works
    
    # # KissFFT - check multiple possible target names
    # if(TARGET kissfft-static)
    #     target_link_libraries(mixmind_services PUBLIC kissfft-static)
    #     target_include_directories(mixmind_services PUBLIC ${kissfft_SOURCE_DIR})
    # elseif(TARGET kissfft::kissfft)
    #     target_link_libraries(mixmind_services PUBLIC kissfft::kissfft)
    # endif()
    # 
    # # libebur128 - uses 'ebur128' as target name
    # if(TARGET ebur128)
    #     target_link_libraries(mixmind_services PUBLIC ebur128)
    #     target_include_directories(mixmind_services PUBLIC ${libebur128_SOURCE_DIR}/ebur128)
    # endif()
    # 
    # # TagLib - check both possible target names
    # if(TARGET tag)
    #     target_link_libraries(mixmind_services PUBLIC tag)
    #     target_include_directories(mixmind_services PUBLIC ${taglib_SOURCE_DIR}/taglib)
    # elseif(TARGET TagLib::tag)
    #     target_link_libraries(mixmind_services PUBLIC TagLib::tag)
    #     target_include_directories(mixmind_services PUBLIC ${taglib_SOURCE_DIR}/taglib)
    # endif()
    # 
    # # SoundTouch - likely uses 'SoundTouch' as target name
    # if(TARGET SoundTouch)
    #     target_link_libraries(mixmind_services PUBLIC SoundTouch)
    #     target_include_directories(mixmind_services PUBLIC ${soundtouch_SOURCE_DIR}/include)
    # elseif(TARGET soundtouch-static)
    #     target_link_libraries(mixmind_services PUBLIC soundtouch-static)
    #     target_include_directories(mixmind_services PUBLIC ${soundtouch_SOURCE_DIR}/include)
    # endif()
    
    # OSC support - disable for now to get build working
    # if(DEFINED OSC_LIBRARY AND TARGET ${OSC_LIBRARY})
    #     target_link_libraries(mixmind_services PUBLIC ${OSC_LIBRARY})
    # endif()

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