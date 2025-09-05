# MixMind AI - Performance Benchmark Suite
# Professional performance validation and regression testing

add_library(MixMindBenchmarks
    PerformanceBenchmark.cpp
    PerformanceBenchmark.h
)

target_link_libraries(MixMindBenchmarks 
    PRIVATE
    MixMindCore
    nlohmann_json::nlohmann_json
)

# Windows-specific libraries for performance monitoring
if(WIN32)
    target_link_libraries(MixMindBenchmarks PRIVATE
        pdh
        psapi
    )
endif()

target_include_directories(MixMindBenchmarks 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_features(MixMindBenchmarks PRIVATE cxx_std_20)

# Optional: Build benchmark executable for standalone testing
if(BUILD_BENCHMARKS)
    add_executable(MixMindBenchmarkRunner
        benchmark_runner.cpp
    )
    
    target_link_libraries(MixMindBenchmarkRunner PRIVATE
        MixMindBenchmarks
        MixMindCore
    )
endif()

# Add to main target for integrated benchmarks
target_link_libraries(MixMindAI PRIVATE MixMindBenchmarks)