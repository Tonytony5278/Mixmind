#pragma once

#include "IOSSService.h"
#include "../core/types.h"
#include "../core/result.h"
#include <kiss_fft.h>
#include <memory>
#include <vector>
#include <complex>
#include <mutex>
#include <atomic>

namespace mixmind::services {

// ============================================================================
// KissFFT Service - Fast Fourier Transform analysis using KissFFT
// ============================================================================

class KissFFTService : public IAudioAnalysisService, public IAudioProcessingService {
public:
    KissFFTService();
    ~KissFFTService() override;
    
    // Non-copyable, movable
    KissFFTService(const KissFFTService&) = delete;
    KissFFTService& operator=(const KissFFTService&) = delete;
    KissFFTService(KissFFTService&&) = default;
    KissFFTService& operator=(KissFFTService&&) = default;
    
    // ========================================================================
    // IOSSService Implementation
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> initialize() override;
    core::AsyncResult<core::VoidResult> shutdown() override;
    bool isInitialized() const override;
    std::string getServiceName() const override;
    std::string getServiceVersion() const override;
    ServiceInfo getServiceInfo() const override;
    core::VoidResult configure(const std::unordered_map<std::string, std::string>& config) override;
    std::optional<std::string> getConfigValue(const std::string& key) const override;
    core::VoidResult resetConfiguration() override;
    bool isHealthy() const override;
    std::string getLastError() const override;
    core::AsyncResult<core::VoidResult> runSelfTest() override;
    PerformanceMetrics getPerformanceMetrics() const override;
    void resetPerformanceMetrics() override;
    
    // ========================================================================
    // IAudioAnalysisService Implementation
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> analyzeBuffer(
        const core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate,
        core::ProgressCallback progress = nullptr
    ) override;
    
    core::AsyncResult<core::VoidResult> analyzeFile(
        const std::string& filePath,
        core::ProgressCallback progress = nullptr
    ) override;
    
    std::unordered_map<std::string, double> getAnalysisResults() const override;
    void clearResults() override;
    bool isAnalyzing() const override;
    core::VoidResult cancelAnalysis() override;
    
    // ========================================================================
    // IAudioProcessingService Implementation
    // ========================================================================
    
    core::VoidResult processBuffer(
        core::FloatAudioBuffer& buffer,
        core::SampleRate sampleRate
    ) override;
    
    core::VoidResult processBuffer(
        const core::FloatAudioBuffer& inputBuffer,
        core::FloatAudioBuffer& outputBuffer,
        core::SampleRate sampleRate
    ) override;
    
    core::VoidResult setParameters(const std::unordered_map<std::string, double>& parameters) override;
    std::unordered_map<std::string, double> getParameters() const override;
    core::VoidResult resetState() override;
    int32_t getLatencySamples() const override;
    
    // ========================================================================
    // FFT Configuration
    // ========================================================================
    
    /// FFT window types
    enum class WindowType {
        Rectangular,    // No windowing
        Hanning,        // Hann window
        Hamming,        // Hamming window
        Blackman,       // Blackman window
        Kaiser,         // Kaiser window (beta parameter)
        Gaussian,       // Gaussian window
        Tukey,          // Tukey window (alpha parameter)
        Bartlett,       // Bartlett (triangular) window
        Welch           // Welch window
    };
    
    /// Set FFT size (must be power of 2)
    core::VoidResult setFFTSize(int32_t fftSize);
    
    /// Get current FFT size
    int32_t getFFTSize() const;
    
    /// Set window type
    core::VoidResult setWindowType(WindowType windowType);
    
    /// Get current window type
    WindowType getWindowType() const;
    
    /// Set window overlap (0.0 - 0.95)
    core::VoidResult setWindowOverlap(float overlap);
    
    /// Get current window overlap
    float getWindowOverlap() const;
    
    /// Enable/disable zero padding
    core::VoidResult setZeroPaddingEnabled(bool enabled);
    
    /// Check if zero padding is enabled
    bool isZeroPaddingEnabled() const;
    
    // ========================================================================
    // Spectrum Analysis
    // ========================================================================
    
    /// Spectrum data structure
    struct SpectrumData {
        std::vector<float> frequencies;      // Frequency bins (Hz)
        std::vector<float> magnitudes;       // Magnitude spectrum (linear)
        std::vector<float> magnitudesDB;     // Magnitude spectrum (dB)
        std::vector<float> phases;           // Phase spectrum (radians)
        std::vector<std::complex<float>> complex; // Complex spectrum
        float sampleRate = 0.0f;
        int32_t fftSize = 0;
        double analysisTime = 0.0;           // Timestamp of analysis
    };
    
    /// Compute power spectrum
    core::Result<SpectrumData> computePowerSpectrum(
        const std::vector<float>& audioData,
        core::SampleRate sampleRate
    );
    
    /// Compute power spectral density
    core::Result<SpectrumData> computePowerSpectralDensity(
        const std::vector<float>& audioData,
        core::SampleRate sampleRate
    );
    
    /// Compute cross-correlation spectrum
    core::Result<SpectrumData> computeCrossSpectrum(
        const std::vector<float>& signal1,
        const std::vector<float>& signal2,
        core::SampleRate sampleRate
    );
    
    /// Compute coherence between two signals
    core::Result<std::vector<float>> computeCoherence(
        const std::vector<float>& signal1,
        const std::vector<float>& signal2,
        core::SampleRate sampleRate
    );
    
    // ========================================================================
    // Real-time Spectrum Analysis
    // ========================================================================
    
    /// Start real-time spectrum analysis
    core::AsyncResult<core::VoidResult> startRealtimeAnalysis(
        core::SampleRate sampleRate,
        int32_t bufferSize = 1024
    );
    
    /// Stop real-time spectrum analysis
    core::AsyncResult<core::VoidResult> stopRealtimeAnalysis();
    
    /// Process real-time audio frame
    core::VoidResult processRealtimeFrame(const float* samples, int32_t sampleCount);
    
    /// Get latest spectrum data
    SpectrumData getLatestSpectrum() const;
    
    /// Check if real-time analysis is active
    bool isRealtimeAnalysisActive() const;
    
    /// Set spectrum callback for real-time analysis
    using SpectrumCallback = std::function<void(const SpectrumData& spectrum)>;
    void setSpectrumCallback(SpectrumCallback callback);
    
    // ========================================================================
    // Advanced FFT Operations
    // ========================================================================
    
    /// Forward FFT (time -> frequency)
    core::Result<std::vector<std::complex<float>>> forwardFFT(const std::vector<float>& timeData);
    
    /// Inverse FFT (frequency -> time)
    core::Result<std::vector<float>> inverseFFT(const std::vector<std::complex<float>>& freqData);
    
    /// Convolution using FFT
    core::Result<std::vector<float>> convolve(
        const std::vector<float>& signal,
        const std::vector<float>& impulse
    );
    
    /// Cross-correlation using FFT
    core::Result<std::vector<float>> crossCorrelate(
        const std::vector<float>& signal1,
        const std::vector<float>& signal2
    );
    
    /// Auto-correlation using FFT
    core::Result<std::vector<float>> autoCorrelate(const std::vector<float>& signal);
    
    // ========================================================================
    // Frequency Domain Filtering
    // ========================================================================
    
    /// Apply frequency domain filter
    core::VoidResult applyFrequencyFilter(
        std::vector<std::complex<float>>& spectrum,
        const std::vector<float>& filterResponse
    );
    
    /// Design lowpass filter response
    std::vector<float> designLowpassFilter(float cutoffFreq, core::SampleRate sampleRate) const;
    
    /// Design highpass filter response
    std::vector<float> designHighpassFilter(float cutoffFreq, core::SampleRate sampleRate) const;
    
    /// Design bandpass filter response
    std::vector<float> designBandpassFilter(
        float lowFreq, 
        float highFreq, 
        core::SampleRate sampleRate
    ) const;
    
    /// Design notch filter response
    std::vector<float> designNotchFilter(
        float centerFreq, 
        float bandwidth, 
        core::SampleRate sampleRate
    ) const;
    
    // ========================================================================
    // Spectral Features Extraction
    // ========================================================================
    
    struct SpectralFeatures {
        float spectralCentroid = 0.0f;       // Brightness (Hz)
        float spectralSpread = 0.0f;         // Bandwidth (Hz)
        float spectralSkewness = 0.0f;       // Asymmetry
        float spectralKurtosis = 0.0f;       // Peakedness
        float spectralRolloff = 0.0f;        // 85% energy frequency
        float spectralFlux = 0.0f;           // Change from previous frame
        float zeroCrossingRate = 0.0f;       // Zero crossings per second
        std::vector<float> mfcc;             // Mel-frequency cepstral coefficients
        std::vector<float> chroma;           // Chroma features
    };
    
    /// Extract spectral features from spectrum
    SpectralFeatures extractSpectralFeatures(
        const SpectrumData& spectrum,
        const SpectralFeatures* previousFeatures = nullptr
    ) const;
    
    /// Compute spectral centroid
    float computeSpectralCentroid(const SpectrumData& spectrum) const;
    
    /// Compute spectral rolloff
    float computeSpectralRolloff(const SpectrumData& spectrum, float percentage = 0.85f) const;
    
    /// Compute spectral flux
    float computeSpectralFlux(
        const SpectrumData& currentSpectrum,
        const SpectrumData& previousSpectrum
    ) const;
    
    // ========================================================================
    // Pitch and Harmonic Analysis
    // ========================================================================
    
    /// Detect fundamental frequency using FFT
    core::Result<float> detectFundamentalFrequency(
        const std::vector<float>& audioData,
        core::SampleRate sampleRate,
        float minFreq = 80.0f,
        float maxFreq = 2000.0f
    );
    
    /// Extract harmonic peaks
    std::vector<std::pair<float, float>> extractHarmonicPeaks(
        const SpectrumData& spectrum,
        int32_t maxPeaks = 20,
        float minMagnitudeDB = -60.0f
    ) const;
    
    /// Compute harmonic-to-noise ratio
    float computeHarmonicToNoiseRatio(
        const SpectrumData& spectrum,
        float fundamentalFreq
    ) const;
    
    // ========================================================================
    // Windowing Functions
    // ========================================================================
    
    /// Apply window function to data
    std::vector<float> applyWindow(
        const std::vector<float>& data,
        WindowType windowType
    ) const;
    
    /// Generate window function
    std::vector<float> generateWindow(int32_t size, WindowType windowType) const;
    
    /// Get window normalization factor
    float getWindowNormalizationFactor(WindowType windowType, int32_t size) const;
    
    // ========================================================================
    // Utility Functions
    // ========================================================================
    
    /// Convert linear magnitude to dB
    static float magnitudeToDecibels(float magnitude, float reference = 1.0f);
    
    /// Convert dB to linear magnitude
    static float decibelsToMagnitude(float decibels, float reference = 1.0f);
    
    /// Convert frequency to mel scale
    static float frequencyToMel(float frequency);
    
    /// Convert mel scale to frequency
    static float melToFrequency(float mel);
    
    /// Check if size is power of 2
    static bool isPowerOfTwo(int32_t size);
    
    /// Find next power of 2
    static int32_t nextPowerOfTwo(int32_t size);
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize KissFFT configuration
    core::VoidResult initializeFFTConfig();
    
    /// Cleanup KissFFT resources
    void cleanupFFTConfig();
    
    /// Prepare input buffer with windowing and padding
    std::vector<kiss_fft_cpx> prepareInputBuffer(const std::vector<float>& input);
    
    /// Process FFT output to spectrum data
    SpectrumData processFFTOutput(
        const std::vector<kiss_fft_cpx>& fftOutput,
        core::SampleRate sampleRate
    );
    
    /// Update performance metrics
    void updatePerformanceMetrics(double processingTime, bool success);
    
    /// Validate FFT parameters
    core::VoidResult validateFFTParameters() const;
    
private:
    // KissFFT configuration
    kiss_fft_cfg forwardConfig_ = nullptr;
    kiss_fft_cfg inverseConfig_ = nullptr;
    std::mutex fftMutex_;
    
    // FFT parameters
    int32_t fftSize_ = 1024;
    WindowType windowType_ = WindowType::Hanning;
    float windowOverlap_ = 0.5f;
    bool zeroPaddingEnabled_ = true;
    std::vector<float> window_;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> isAnalyzing_{false};
    std::atomic<bool> shouldCancel_{false};
    std::atomic<bool> isRealtimeActive_{false};
    
    // Configuration
    std::unordered_map<std::string, std::string> config_;
    std::mutex configMutex_;
    
    // Analysis results
    mutable std::unordered_map<std::string, double> analysisResults_;
    mutable std::mutex resultsMutex_;
    
    // Real-time analysis
    std::vector<float> realtimeBuffer_;
    SpectrumData latestSpectrum_;
    SpectrumCallback spectrumCallback_;
    std::mutex realtimeMutex_;
    
    // Performance tracking
    mutable PerformanceMetrics performanceMetrics_;
    mutable std::mutex metricsMutex_;
    
    // Error tracking
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;
};

} // namespace mixmind::services