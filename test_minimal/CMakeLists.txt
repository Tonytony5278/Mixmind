cmake_minimum_required(VERSION 3.20)
project(MixMindCore_Test VERSION 1.0.0 LANGUAGES CXX)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create test executable with just our core files
add_executable(core_test
    ../src/core/async.cpp
    ../src/core/types.h
    ../src/core/result.h
    ../src/core/async.h
    minimal_test.cpp
)

target_include_directories(core_test PRIVATE
    ../src/core
    ../src
)

# Platform-specific settings for Windows
if(WIN32)
    target_compile_definitions(core_test PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Enable threading
find_package(Threads REQUIRED)
target_link_libraries(core_test PRIVATE Threads::Threads)