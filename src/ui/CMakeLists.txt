# MixMind UI Components
cmake_minimum_required(VERSION 3.15)

# Add UI source files
set(UI_SOURCES
    MixMindMainWindow.cpp
    MixerPanel.cpp
    PluginRackPanel.cpp
    AutomationEditor.cpp
    AIAssistantPanel.cpp
    PerformanceMonitorPanel.cpp
)

set(UI_HEADERS
    MixMindMainWindow.h
    WebUI.h
)

# Create UI library
add_library(mixmind_ui STATIC ${UI_SOURCES} ${UI_HEADERS})

# Include directories
target_include_directories(mixmind_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link with core libraries
target_link_libraries(mixmind_ui
    mixmind_core
    mixmind_audio
    mixmind_plugins
    mixmind_automation
    mixmind_performance
    mixmind_services
)

# Dear ImGui integration - conditional compilation
if(MIXMIND_ENABLE_GUI)
    # Try to find Dear ImGui
    find_path(IMGUI_INCLUDE_DIR imgui.h
        HINTS
        ${CMAKE_SOURCE_DIR}/external/imgui
        ${CMAKE_SOURCE_DIR}/third_party/imgui
        /usr/include/imgui
        /usr/local/include/imgui
    )

    if(IMGUI_INCLUDE_DIR)
        message(STATUS "Found Dear ImGui at: ${IMGUI_INCLUDE_DIR}")
        
        # Add ImGui sources
        set(IMGUI_SOURCES
            ${IMGUI_INCLUDE_DIR}/imgui.cpp
            ${IMGUI_INCLUDE_DIR}/imgui_demo.cpp
            ${IMGUI_INCLUDE_DIR}/imgui_draw.cpp
            ${IMGUI_INCLUDE_DIR}/imgui_tables.cpp
            ${IMGUI_INCLUDE_DIR}/imgui_widgets.cpp
            ${IMGUI_INCLUDE_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_INCLUDE_DIR}/backends/imgui_impl_opengl3.cpp
        )
        
        target_sources(mixmind_ui PRIVATE ${IMGUI_SOURCES})
        target_include_directories(mixmind_ui PRIVATE ${IMGUI_INCLUDE_DIR})
        target_include_directories(mixmind_ui PRIVATE ${IMGUI_INCLUDE_DIR}/backends)
        
        # Enable ImGui features
        target_compile_definitions(mixmind_ui PRIVATE 
            IMGUI_IMPL_OPENGL_LOADER_GL3W=1
            MIXMIND_UI_ENABLED=1
        )
        
    else()
        message(WARNING "Dear ImGui not found. Building without GUI support.")
        target_compile_definitions(mixmind_ui PRIVATE MIXMIND_UI_DISABLED=1)
    endif()

    # OpenGL and GLFW
    find_package(OpenGL QUIET)
    find_package(glfw3 QUIET)

    if(OpenGL_FOUND)
        target_link_libraries(mixmind_ui ${OPENGL_LIBRARIES})
        target_include_directories(mixmind_ui PRIVATE ${OPENGL_INCLUDE_DIRS})
    endif()

    if(glfw3_FOUND)
        target_link_libraries(mixmind_ui glfw)
        message(STATUS "Found GLFW3")
    else()
        # Try to find GLFW manually
        find_library(GLFW_LIBRARY 
            NAMES glfw glfw3
            HINTS 
            ${CMAKE_SOURCE_DIR}/external/glfw/lib
            /usr/lib
            /usr/local/lib
        )
        
        find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h
            HINTS
            ${CMAKE_SOURCE_DIR}/external/glfw/include
            /usr/include
            /usr/local/include
        )
        
        if(GLFW_LIBRARY AND GLFW_INCLUDE_DIR)
            target_link_libraries(mixmind_ui ${GLFW_LIBRARY})
            target_include_directories(mixmind_ui PRIVATE ${GLFW_INCLUDE_DIR})
            message(STATUS "Found GLFW at: ${GLFW_LIBRARY}")
        endif()
    endif()

    # GL3W for OpenGL loading
    find_path(GL3W_INCLUDE_DIR GL/gl3w.h
        HINTS
        ${CMAKE_SOURCE_DIR}/external/gl3w/include
        ${IMGUI_INCLUDE_DIR}/examples/libs/gl3w
        /usr/include
        /usr/local/include
    )

    if(GL3W_INCLUDE_DIR)
        # Add GL3W source
        if(EXISTS ${GL3W_INCLUDE_DIR}/../src/gl3w.c)
            target_sources(mixmind_ui PRIVATE ${GL3W_INCLUDE_DIR}/../src/gl3w.c)
        elseif(EXISTS ${CMAKE_SOURCE_DIR}/external/gl3w/src/gl3w.c)
            target_sources(mixmind_ui PRIVATE ${CMAKE_SOURCE_DIR}/external/gl3w/src/gl3w.c)
        endif()
        
        target_include_directories(mixmind_ui PRIVATE ${GL3W_INCLUDE_DIR})
        message(STATUS "Found GL3W at: ${GL3W_INCLUDE_DIR}")
    else()
        target_compile_definitions(mixmind_ui PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM=1)
    endif()

    # Platform-specific libraries
    if(WIN32)
        target_link_libraries(mixmind_ui 
            opengl32
            gdi32
            shell32
            user32
            kernel32
        )
    elseif(APPLE)
        find_library(COCOA_LIBRARY Cocoa)
        find_library(IOKIT_LIBRARY IOKit)
        find_library(COREVIDEO_LIBRARY CoreVideo)
        
        if(COCOA_LIBRARY AND IOKIT_LIBRARY AND COREVIDEO_LIBRARY)
            target_link_libraries(mixmind_ui 
                ${COCOA_LIBRARY}
                ${IOKIT_LIBRARY}
                ${COREVIDEO_LIBRARY}
            )
        endif()
    elseif(UNIX)
        find_package(X11 QUIET)
        if(X11_FOUND)
            target_link_libraries(mixmind_ui 
                ${X11_LIBRARIES}
                ${CMAKE_DL_LIBS}
            )
        endif()
    endif()
else()
    # Minimal UI mode - only compile headers
    target_compile_definitions(mixmind_ui PRIVATE MIXMIND_UI_DISABLED=1)
endif()

# Compiler-specific settings
target_compile_features(mixmind_ui PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(mixmind_ui PRIVATE /W4)
    # Disable specific warnings for ImGui
    target_compile_options(mixmind_ui PRIVATE 
        /wd4127  # conditional expression is constant
        /wd4996  # deprecated functions
    )
else()
    target_compile_options(mixmind_ui PRIVATE -Wall -Wextra)
    # Disable specific warnings for ImGui
    target_compile_options(mixmind_ui PRIVATE 
        -Wno-unused-parameter
        -Wno-format-security
    )
endif()

# Link to main application
if(TARGET MixMindAI)
    target_link_libraries(MixMindAI PRIVATE mixmind_ui)
endif()