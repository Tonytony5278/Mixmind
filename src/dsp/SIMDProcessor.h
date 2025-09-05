#pragma once

#include <immintrin.h>  // AVX2/SSE instructions
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>

namespace mixmind::dsp {

// SIMD capability detection
class SIMDCapabilities {
public:
    static SIMDCapabilities& getInstance();
    
    bool hasSSE() const { return hasSSE_; }
    bool hasSSE2() const { return hasSSE2_; }
    bool hasSSE3() const { return hasSSE3_; }
    bool hasSSE41() const { return hasSSE41_; }
    bool hasSSE42() const { return hasSSE42_; }
    bool hasAVX() const { return hasAVX_; }
    bool hasAVX2() const { return hasAVX2_; }
    bool hasFMA() const { return hasFMA_; }
    bool hasAVX512() const { return hasAVX512_; }
    
    void printCapabilities() const;
    
private:
    SIMDCapabilities();
    void detectCapabilities();
    
    bool hasSSE_ = false;
    bool hasSSE2_ = false;
    bool hasSSE3_ = false;
    bool hasSSE41_ = false;
    bool hasSSE42_ = false;
    bool hasAVX_ = false;
    bool hasAVX2_ = false;
    bool hasFMA_ = false;
    bool hasAVX512_ = false;
};

// Aligned memory allocator for SIMD operations
template<typename T, size_t Alignment = 32>
class AlignedAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    
    template<typename U>
    struct rebind { using other = AlignedAllocator<U, Alignment>; };
    
    AlignedAllocator() = default;
    template<typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) {}
    
    pointer allocate(size_type n) {
        if (n == 0) return nullptr;
        
        void* ptr = nullptr;
#ifdef _WIN32
        ptr = _aligned_malloc(n * sizeof(T), Alignment);
#else
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) {
            ptr = nullptr;
        }
#endif
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(ptr);
    }
    
    void deallocate(pointer p, size_type) {
        if (p) {
#ifdef _WIN32
            _aligned_free(p);
#else
            free(p);
#endif
        }
    }
    
    template<typename U>
    bool operator==(const AlignedAllocator<U, Alignment>&) const { return true; }
    
    template<typename U>
    bool operator!=(const AlignedAllocator<U, Alignment>&) const { return false; }
};

// Aligned vector for SIMD operations
template<typename T>
using AlignedVector = std::vector<T, AlignedAllocator<T, 32>>;

// High-performance audio processing with SIMD optimization
class SIMDProcessor {
public:
    SIMDProcessor();
    ~SIMDProcessor() = default;
    
    // Basic audio operations (optimized with AVX2/SSE)
    static void add(const float* input1, const float* input2, float* output, size_t count);
    static void subtract(const float* input1, const float* input2, float* output, size_t count);
    static void multiply(const float* input1, const float* input2, float* output, size_t count);
    static void multiplyConstant(const float* input, float constant, float* output, size_t count);
    static void addConstant(const float* input, float constant, float* output, size_t count);
    
    // Advanced audio operations
    static void mix(const float* input1, const float* input2, float* output, 
                   float gain1, float gain2, size_t count);
    static void copy(const float* input, float* output, size_t count);
    static void clear(float* buffer, size_t count);
    static void applyGain(float* buffer, float gain, size_t count);
    static void fadeIn(float* buffer, size_t count);
    static void fadeOut(float* buffer, size_t count);
    static void crossfade(const float* input1, const float* input2, float* output, 
                         float crossfadePos, size_t count);
    
    // Audio analysis operations
    static float findPeak(const float* input, size_t count);
    static float calculateRMS(const float* input, size_t count);
    static double calculateSum(const float* input, size_t count);
    static void findMinMax(const float* input, size_t count, float& min, float& max);
    
    // Filter operations (optimized)
    class SIMDFilter {
    public:
        virtual ~SIMDFilter() = default;
        virtual void process(const float* input, float* output, size_t count) = 0;
        virtual void reset() = 0;
    };
    
    // Biquad filter with SIMD optimization
    class SIMDBiquad : public SIMDFilter {
    public:
        enum Type {
            LowPass,
            HighPass,
            BandPass,
            Notch,
            AllPass,
            Peaking,
            LowShelf,
            HighShelf
        };
        
        SIMDBiquad(Type type = LowPass, float frequency = 1000.0f, 
                   float sampleRate = 48000.0f, float Q = 0.707f, float gain = 0.0f);
        
        void setParameters(Type type, float frequency, float sampleRate, 
                          float Q = 0.707f, float gain = 0.0f);
        void process(const float* input, float* output, size_t count) override;
        void reset() override;
        
    private:
        void calculateCoefficients();
        
        // Filter coefficients (aligned for SIMD)
        alignas(32) float b0_, b1_, b2_, a1_, a2_;
        
        // Filter state (aligned for SIMD)
        alignas(32) float x1_[8] = {0}; // Support for 8-channel SIMD processing
        alignas(32) float x2_[8] = {0};
        alignas(32) float y1_[8] = {0};
        alignas(32) float y2_[8] = {0};
        
        Type type_;
        float frequency_;
        float sampleRate_;
        float Q_;
        float gain_;
    };
    
    // Convolution with SIMD optimization
    class SIMDConvolution {
    public:
        SIMDConvolution(const float* impulseResponse, size_t impulseLength);
        ~SIMDConvolution();
        
        void process(const float* input, float* output, size_t count);
        void reset();
        
    private:
        void setupFFTConvolution();
        
        AlignedVector<float> impulseResponse_;
        AlignedVector<float> delayLine_;
        size_t impulseLength_;
        size_t delayIndex_;
        
        // FFT-based convolution for longer impulses
        bool useFFT_;
        static constexpr size_t FFT_THRESHOLD = 128;
    };
    
    // Compressor/Limiter with SIMD optimization
    class SIMDCompressor {
    public:
        struct Parameters {
            float threshold = -12.0f;      // dB
            float ratio = 4.0f;            // compression ratio
            float attack = 10.0f;          // ms
            float release = 100.0f;        // ms
            float knee = 2.0f;             // dB (soft knee)
            float makeupGain = 0.0f;       // dB
            bool autoMakeup = true;
        };
        
        SIMDCompressor(float sampleRate = 48000.0f);
        
        void setParameters(const Parameters& params);
        void process(const float* input, float* output, size_t count);
        void processStereo(const float* inputL, const float* inputR, 
                          float* outputL, float* outputR, size_t count);
        void reset();
        
        // Get compression meters
        float getGainReduction() const { return gainReduction_; }
        float getInputLevel() const { return inputLevel_; }
        float getOutputLevel() const { return outputLevel_; }
        
    private:
        void calculateEnvelope(float input, float& envelope);
        float calculateGainReduction(float level);
        
        Parameters params_;
        float sampleRate_;
        
        // Envelope followers (aligned for SIMD)
        alignas(32) float envelope_;
        alignas(32) float attackCoeff_;
        alignas(32) float releaseCoeff_;
        
        // Metering
        float gainReduction_;
        float inputLevel_;
        float outputLevel_;
    };
    
    // Reverb with SIMD optimization
    class SIMDReverb {
    public:
        struct Parameters {
            float roomSize = 0.5f;         // 0.0 - 1.0
            float damping = 0.5f;          // 0.0 - 1.0
            float width = 1.0f;            // 0.0 - 1.0
            float wetLevel = 0.3f;         // 0.0 - 1.0
            float dryLevel = 0.7f;         // 0.0 - 1.0
            float predelay = 0.0f;         // ms
        };
        
        SIMDReverb(float sampleRate = 48000.0f);
        ~SIMDReverb();
        
        void setParameters(const Parameters& params);
        void processStereo(const float* inputL, const float* inputR,
                          float* outputL, float* outputR, size_t count);
        void reset();
        
    private:
        class AllPassFilter;
        class CombFilter;
        
        void setupFilters();
        
        Parameters params_;
        float sampleRate_;
        
        // Freeverb-style reverb structure
        static constexpr int NUM_COMBS = 8;
        static constexpr int NUM_ALLPASSES = 4;
        
        std::unique_ptr<CombFilter> combFilters_[NUM_COMBS];
        std::unique_ptr<AllPassFilter> allPassFilters_[NUM_ALLPASSES];
        
        AlignedVector<float> predelayBuffer_;
        size_t predelayIndex_;
        size_t predelayLength_;
    };
    
    // Utility functions for SIMD processing
    static bool isAligned(const void* ptr, size_t alignment = 32);
    static size_t getAlignedSize(size_t size, size_t alignment = 32);
    static void prefetchMemory(const void* ptr, size_t size);
    
    // Performance profiling
    struct ProcessingStats {
        double totalTime = 0.0;
        size_t totalSamples = 0;
        double avgTimePerSample = 0.0;
        double peakTime = 0.0;
        size_t callCount = 0;
    };
    
    static void startProfiling(const char* name);
    static void endProfiling(const char* name);
    static const ProcessingStats& getStats(const char* name);
    static void resetStats();
    static void printStats();
    
private:
    static SIMDCapabilities capabilities_;
    
    // Function pointers for optimized implementations
    using AddFunction = void(*)(const float*, const float*, float*, size_t);
    using MultiplyFunction = void(*)(const float*, const float*, float*, size_t);
    using PeakFunction = float(*)(const float*, size_t);
    
    static AddFunction addFunc_;
    static MultiplyFunction multiplyFunc_;
    static PeakFunction peakFunc_;
    
    // Implementation variants
    static void addSSE(const float* input1, const float* input2, float* output, size_t count);
    static void addAVX(const float* input1, const float* input2, float* output, size_t count);
    static void addAVX2(const float* input1, const float* input2, float* output, size_t count);
    
    static void multiplySSE(const float* input1, const float* input2, float* output, size_t count);
    static void multiplyAVX(const float* input1, const float* input2, float* output, size_t count);
    static void multiplyAVX2(const float* input1, const float* input2, float* output, size_t count);
    
    static float findPeakSSE(const float* input, size_t count);
    static float findPeakAVX(const float* input, size_t count);
    static float findPeakAVX2(const float* input, size_t count);
    
    // Initialize function pointers based on CPU capabilities
    static void initializeFunctions();
    static bool initialized_;
};

// RAII profiler for automatic timing
class SIMDProfiler {
public:
    explicit SIMDProfiler(const char* name) : name_(name) {
        SIMDProcessor::startProfiling(name_);
    }
    
    ~SIMDProfiler() {
        SIMDProcessor::endProfiling(name_);
    }
    
private:
    const char* name_;
};

#define SIMD_PROFILE(name) SIMDProfiler prof(name)

} // namespace mixmind::dsp