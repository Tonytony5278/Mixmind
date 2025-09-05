#include "SIMDProcessor.h"
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <cmath>

#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace mixmind::dsp {

// Static member initialization
SIMDCapabilities SIMDProcessor::capabilities_;
bool SIMDProcessor::initialized_ = false;
SIMDProcessor::AddFunction SIMDProcessor::addFunc_ = nullptr;
SIMDProcessor::MultiplyFunction SIMDProcessor::multiplyFunc_ = nullptr;
SIMDProcessor::PeakFunction SIMDProcessor::peakFunc_ = nullptr;

// Profiling data
static std::unordered_map<std::string, SIMDProcessor::ProcessingStats> g_stats;
static std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> g_startTimes;
static std::mutex g_statsMutex;

// ============================================================================
// SIMD Capabilities Detection
// ============================================================================

SIMDCapabilities& SIMDCapabilities::getInstance() {
    static SIMDCapabilities instance;
    return instance;
}

SIMDCapabilities::SIMDCapabilities() {
    detectCapabilities();
}

void SIMDCapabilities::detectCapabilities() {
    std::array<int, 4> cpuid_data{};
    
#ifdef _WIN32
    __cpuid(cpuid_data.data(), 1);
#else
    __get_cpuid(1, reinterpret_cast<unsigned int*>(&cpuid_data[0]), 
                   reinterpret_cast<unsigned int*>(&cpuid_data[1]),
                   reinterpret_cast<unsigned int*>(&cpuid_data[2]), 
                   reinterpret_cast<unsigned int*>(&cpuid_data[3]));
#endif
    
    // Check CPU features
    hasSSE_ = (cpuid_data[3] & (1 << 25)) != 0;
    hasSSE2_ = (cpuid_data[3] & (1 << 26)) != 0;
    hasSSE3_ = (cpuid_data[2] & (1 << 0)) != 0;
    hasSSE41_ = (cpuid_data[2] & (1 << 19)) != 0;
    hasSSE42_ = (cpuid_data[2] & (1 << 20)) != 0;
    hasAVX_ = (cpuid_data[2] & (1 << 28)) != 0;
    hasFMA_ = (cpuid_data[2] & (1 << 12)) != 0;
    
    // Check for AVX2
    if (hasAVX_) {
#ifdef _WIN32
        __cpuidex(cpuid_data.data(), 7, 0);
#else
        __get_cpuid_count(7, 0, reinterpret_cast<unsigned int*>(&cpuid_data[0]), 
                             reinterpret_cast<unsigned int*>(&cpuid_data[1]),
                             reinterpret_cast<unsigned int*>(&cpuid_data[2]), 
                             reinterpret_cast<unsigned int*>(&cpuid_data[3]));
#endif
        hasAVX2_ = (cpuid_data[1] & (1 << 5)) != 0;
        hasAVX512_ = (cpuid_data[1] & (1 << 16)) != 0;
    }
}

void SIMDCapabilities::printCapabilities() const {
    std::cout << "SIMD Capabilities:\n";
    std::cout << "  SSE: " << (hasSSE_ ? "YES" : "NO") << "\n";
    std::cout << "  SSE2: " << (hasSSE2_ ? "YES" : "NO") << "\n";
    std::cout << "  SSE3: " << (hasSSE3_ ? "YES" : "NO") << "\n";
    std::cout << "  SSE4.1: " << (hasSSE41_ ? "YES" : "NO") << "\n";
    std::cout << "  SSE4.2: " << (hasSSE42_ ? "YES" : "NO") << "\n";
    std::cout << "  AVX: " << (hasAVX_ ? "YES" : "NO") << "\n";
    std::cout << "  AVX2: " << (hasAVX2_ ? "YES" : "NO") << "\n";
    std::cout << "  FMA: " << (hasFMA_ ? "YES" : "NO") << "\n";
    std::cout << "  AVX512: " << (hasAVX512_ ? "YES" : "NO") << "\n";
}

// ============================================================================
// SIMD Processor Implementation
// ============================================================================

SIMDProcessor::SIMDProcessor() {
    if (!initialized_) {
        initializeFunctions();
        initialized_ = true;
    }
}

void SIMDProcessor::initializeFunctions() {
    capabilities_ = SIMDCapabilities::getInstance();
    
    // Select best available implementations
    if (capabilities_.hasAVX2()) {
        addFunc_ = addAVX2;
        multiplyFunc_ = multiplyAVX2;
        peakFunc_ = findPeakAVX2;
        std::cout << "Using AVX2 optimizations\n";
    } else if (capabilities_.hasAVX()) {
        addFunc_ = addAVX;
        multiplyFunc_ = multiplyAVX;
        peakFunc_ = findPeakAVX;
        std::cout << "Using AVX optimizations\n";
    } else if (capabilities_.hasSSE2()) {
        addFunc_ = addSSE;
        multiplyFunc_ = multiplySSE;
        peakFunc_ = findPeakSSE;
        std::cout << "Using SSE2 optimizations\n";
    } else {
        std::cout << "No SIMD optimizations available, using scalar fallback\n";
    }
}

// ============================================================================
// Basic Audio Operations
// ============================================================================

void SIMDProcessor::add(const float* input1, const float* input2, float* output, size_t count) {
    if (addFunc_) {
        addFunc_(input1, input2, output, count);
    } else {
        // Scalar fallback
        for (size_t i = 0; i < count; ++i) {
            output[i] = input1[i] + input2[i];
        }
    }
}

void SIMDProcessor::multiply(const float* input1, const float* input2, float* output, size_t count) {
    if (multiplyFunc_) {
        multiplyFunc_(input1, input2, output, count);
    } else {
        // Scalar fallback
        for (size_t i = 0; i < count; ++i) {
            output[i] = input1[i] * input2[i];
        }
    }
}

void SIMDProcessor::multiplyConstant(const float* input, float constant, float* output, size_t count) {
    SIMD_PROFILE("multiplyConstant");
    
    if (capabilities_.hasAVX2()) {
        const __m256 constantVec = _mm256_set1_ps(constant);
        const size_t simdCount = count & ~7; // Process 8 at a time
        
        for (size_t i = 0; i < simdCount; i += 8) {
            __m256 inputVec = _mm256_load_ps(&input[i]);
            __m256 result = _mm256_mul_ps(inputVec, constantVec);
            _mm256_store_ps(&output[i], result);
        }
        
        // Handle remaining samples
        for (size_t i = simdCount; i < count; ++i) {
            output[i] = input[i] * constant;
        }
    } else if (capabilities_.hasSSE2()) {
        const __m128 constantVec = _mm_set1_ps(constant);
        const size_t simdCount = count & ~3; // Process 4 at a time
        
        for (size_t i = 0; i < simdCount; i += 4) {
            __m128 inputVec = _mm_load_ps(&input[i]);
            __m128 result = _mm_mul_ps(inputVec, constantVec);
            _mm_store_ps(&output[i], result);
        }
        
        // Handle remaining samples
        for (size_t i = simdCount; i < count; ++i) {
            output[i] = input[i] * constant;
        }
    } else {
        // Scalar fallback
        for (size_t i = 0; i < count; ++i) {
            output[i] = input[i] * constant;
        }
    }
}

void SIMDProcessor::applyGain(float* buffer, float gain, size_t count) {
    SIMD_PROFILE("applyGain");
    multiplyConstant(buffer, gain, buffer, count);
}

void SIMDProcessor::clear(float* buffer, size_t count) {
    SIMD_PROFILE("clear");
    
    if (capabilities_.hasAVX2()) {
        const __m256 zero = _mm256_setzero_ps();
        const size_t simdCount = count & ~7;
        
        for (size_t i = 0; i < simdCount; i += 8) {
            _mm256_store_ps(&buffer[i], zero);
        }
        
        // Clear remaining samples
        for (size_t i = simdCount; i < count; ++i) {
            buffer[i] = 0.0f;
        }
    } else if (capabilities_.hasSSE2()) {
        const __m128 zero = _mm_setzero_ps();
        const size_t simdCount = count & ~3;
        
        for (size_t i = 0; i < simdCount; i += 4) {
            _mm_store_ps(&buffer[i], zero);
        }
        
        // Clear remaining samples
        for (size_t i = simdCount; i < count; ++i) {
            buffer[i] = 0.0f;
        }
    } else {
        std::memset(buffer, 0, count * sizeof(float));
    }
}

float SIMDProcessor::findPeak(const float* input, size_t count) {
    if (peakFunc_) {
        return peakFunc_(input, count);
    } else {
        // Scalar fallback
        float peak = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            peak = std::max(peak, std::abs(input[i]));
        }
        return peak;
    }
}

float SIMDProcessor::calculateRMS(const float* input, size_t count) {
    SIMD_PROFILE("calculateRMS");
    
    if (count == 0) return 0.0f;
    
    double sum = 0.0;
    
    if (capabilities_.hasAVX2()) {
        __m256d sumVec = _mm256_setzero_pd();
        const size_t simdCount = count & ~7;
        
        for (size_t i = 0; i < simdCount; i += 8) {
            __m256 samples = _mm256_load_ps(&input[i]);
            __m256 squared = _mm256_mul_ps(samples, samples);
            
            // Convert to double precision and accumulate
            __m128 lo = _mm256_extractf128_ps(squared, 0);
            __m128 hi = _mm256_extractf128_ps(squared, 1);
            
            __m256d lo_d = _mm256_cvtps_pd(lo);
            __m256d hi_d = _mm256_cvtps_pd(hi);
            
            sumVec = _mm256_add_pd(sumVec, lo_d);
            sumVec = _mm256_add_pd(sumVec, hi_d);
        }
        
        // Extract sum from vector
        alignas(32) double sumArray[4];
        _mm256_store_pd(sumArray, sumVec);
        sum = sumArray[0] + sumArray[1] + sumArray[2] + sumArray[3];
        
        // Handle remaining samples
        for (size_t i = simdCount; i < count; ++i) {
            sum += static_cast<double>(input[i]) * input[i];
        }
    } else {
        // Scalar fallback
        for (size_t i = 0; i < count; ++i) {
            sum += static_cast<double>(input[i]) * input[i];
        }
    }
    
    return static_cast<float>(std::sqrt(sum / count));
}

// ============================================================================
// AVX2 Implementations
// ============================================================================

void SIMDProcessor::addAVX2(const float* input1, const float* input2, float* output, size_t count) {
    const size_t simdCount = count & ~7; // Process 8 floats at a time
    
    for (size_t i = 0; i < simdCount; i += 8) {
        __m256 vec1 = _mm256_load_ps(&input1[i]);
        __m256 vec2 = _mm256_load_ps(&input2[i]);
        __m256 result = _mm256_add_ps(vec1, vec2);
        _mm256_store_ps(&output[i], result);
    }
    
    // Handle remaining elements
    for (size_t i = simdCount; i < count; ++i) {
        output[i] = input1[i] + input2[i];
    }
}

void SIMDProcessor::multiplyAVX2(const float* input1, const float* input2, float* output, size_t count) {
    const size_t simdCount = count & ~7;
    
    for (size_t i = 0; i < simdCount; i += 8) {
        __m256 vec1 = _mm256_load_ps(&input1[i]);
        __m256 vec2 = _mm256_load_ps(&input2[i]);
        __m256 result = _mm256_mul_ps(vec1, vec2);
        _mm256_store_ps(&output[i], result);
    }
    
    // Handle remaining elements
    for (size_t i = simdCount; i < count; ++i) {
        output[i] = input1[i] * input2[i];
    }
}

float SIMDProcessor::findPeakAVX2(const float* input, size_t count) {
    __m256 maxVec = _mm256_setzero_ps();
    const size_t simdCount = count & ~7;
    
    for (size_t i = 0; i < simdCount; i += 8) {
        __m256 vec = _mm256_load_ps(&input[i]);
        vec = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), vec); // abs(vec)
        maxVec = _mm256_max_ps(maxVec, vec);
    }
    
    // Extract maximum from vector
    alignas(32) float maxArray[8];
    _mm256_store_ps(maxArray, maxVec);
    
    float peak = 0.0f;
    for (int i = 0; i < 8; ++i) {
        peak = std::max(peak, maxArray[i]);
    }
    
    // Handle remaining elements
    for (size_t i = simdCount; i < count; ++i) {
        peak = std::max(peak, std::abs(input[i]));
    }
    
    return peak;
}

// ============================================================================
// AVX Implementations (fallback)
// ============================================================================

void SIMDProcessor::addAVX(const float* input1, const float* input2, float* output, size_t count) {
    addAVX2(input1, input2, output, count); // AVX2 code works for AVX
}

void SIMDProcessor::multiplyAVX(const float* input1, const float* input2, float* output, size_t count) {
    multiplyAVX2(input1, input2, output, count);
}

float SIMDProcessor::findPeakAVX(const float* input, size_t count) {
    return findPeakAVX2(input, count);
}

// ============================================================================
// SSE Implementations (fallback)
// ============================================================================

void SIMDProcessor::addSSE(const float* input1, const float* input2, float* output, size_t count) {
    const size_t simdCount = count & ~3; // Process 4 floats at a time
    
    for (size_t i = 0; i < simdCount; i += 4) {
        __m128 vec1 = _mm_load_ps(&input1[i]);
        __m128 vec2 = _mm_load_ps(&input2[i]);
        __m128 result = _mm_add_ps(vec1, vec2);
        _mm_store_ps(&output[i], result);
    }
    
    // Handle remaining elements
    for (size_t i = simdCount; i < count; ++i) {
        output[i] = input1[i] + input2[i];
    }
}

void SIMDProcessor::multiplySSE(const float* input1, const float* input2, float* output, size_t count) {
    const size_t simdCount = count & ~3;
    
    for (size_t i = 0; i < simdCount; i += 4) {
        __m128 vec1 = _mm_load_ps(&input1[i]);
        __m128 vec2 = _mm_load_ps(&input2[i]);
        __m128 result = _mm_mul_ps(vec1, vec2);
        _mm_store_ps(&output[i], result);
    }
    
    // Handle remaining elements
    for (size_t i = simdCount; i < count; ++i) {
        output[i] = input1[i] * input2[i];
    }
}

float SIMDProcessor::findPeakSSE(const float* input, size_t count) {
    __m128 maxVec = _mm_setzero_ps();
    const size_t simdCount = count & ~3;
    
    for (size_t i = 0; i < simdCount; i += 4) {
        __m128 vec = _mm_load_ps(&input[i]);
        
        // Get absolute values
        const __m128 signMask = _mm_set1_ps(-0.0f);
        vec = _mm_andnot_ps(signMask, vec);
        
        maxVec = _mm_max_ps(maxVec, vec);
    }
    
    // Extract maximum from vector
    alignas(16) float maxArray[4];
    _mm_store_ps(maxArray, maxVec);
    
    float peak = 0.0f;
    for (int i = 0; i < 4; ++i) {
        peak = std::max(peak, maxArray[i]);
    }
    
    // Handle remaining elements
    for (size_t i = simdCount; i < count; ++i) {
        peak = std::max(peak, std::abs(input[i]));
    }
    
    return peak;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool SIMDProcessor::isAligned(const void* ptr, size_t alignment) {
    return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
}

size_t SIMDProcessor::getAlignedSize(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

void SIMDProcessor::prefetchMemory(const void* ptr, size_t size) {
    const char* p = static_cast<const char*>(ptr);
    const size_t cacheLineSize = 64; // Typical cache line size
    
    for (size_t i = 0; i < size; i += cacheLineSize) {
#ifdef _WIN32
        _mm_prefetch(p + i, _MM_HINT_T0);
#else
        __builtin_prefetch(p + i, 0, 3);
#endif
    }
}

// ============================================================================
// Performance Profiling
// ============================================================================

void SIMDProcessor::startProfiling(const char* name) {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    g_startTimes[name] = std::chrono::high_resolution_clock::now();
}

void SIMDProcessor::endProfiling(const char* name) {
    auto endTime = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(g_statsMutex);
    
    auto startIt = g_startTimes.find(name);
    if (startIt == g_startTimes.end()) return;
    
    auto duration = std::chrono::duration<double, std::milli>(endTime - startIt->second).count();
    
    auto& stats = g_stats[name];
    stats.totalTime += duration;
    stats.callCount++;
    stats.peakTime = std::max(stats.peakTime, duration);
    
    g_startTimes.erase(startIt);
}

const SIMDProcessor::ProcessingStats& SIMDProcessor::getStats(const char* name) {
    static ProcessingStats emptyStats;
    
    std::lock_guard<std::mutex> lock(g_statsMutex);
    auto it = g_stats.find(name);
    return (it != g_stats.end()) ? it->second : emptyStats;
}

void SIMDProcessor::resetStats() {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    g_stats.clear();
    g_startTimes.clear();
}

void SIMDProcessor::printStats() {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    
    std::cout << "\nSIMD Performance Statistics:\n";
    std::cout << "=============================\n";
    
    for (const auto& [name, stats] : g_stats) {
        if (stats.callCount > 0) {
            double avgTime = stats.totalTime / stats.callCount;
            std::cout << name << ":\n";
            std::cout << "  Calls: " << stats.callCount << "\n";
            std::cout << "  Total Time: " << stats.totalTime << " ms\n";
            std::cout << "  Avg Time: " << avgTime << " ms\n";
            std::cout << "  Peak Time: " << stats.peakTime << " ms\n\n";
        }
    }
}

} // namespace mixmind::dsp