# Core implementation library (changed from INTERFACE to static library)
add_library(mixmind_core STATIC)

# Core interface headers and implementation
target_sources(mixmind_core PRIVATE
    # Implementation files
    async.cpp
    
    # Header files
    types.h
    result.h
    async.h
    ISession.h
    ITransport.h
    ITrack.h
    IClip.h
    IPluginHost.h
    IPluginInstance.h
    IAutomation.h
    IRenderService.h
    IMediaLibrary.h
    IAudioProcessor.h
    IAsyncService.h
)

target_include_directories(mixmind_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(mixmind_core INTERFACE cxx_std_20)

# Link core interface to main application
target_link_libraries(MixMindAI PRIVATE mixmind_core)