#pragma once

#include "IOSSService.h"
#include "../core/types.h"
#include "../core/result.h"
#include <ebur128.h>
#include <memory>
#include <mutex>
#include <atomic>

namespace mixmind::services {

// ============================================================================
// libebur128 Service - LUFS and True Peak analysis using libebur128
// ============================================================================

class LibEBU128Service : public IAudioAnalysisService {
public:
    LibEBU128Service();
    ~LibEBU128Service() override;
    
    // Non-copyable, movable
    LibEBU128Service(const LibEBU128Service&) = delete;
    LibEBU128Service& operator=(const LibEBU128Service&) = delete;
    LibEBU128Service(LibEBU128Service&&) = default;
    LibEBU128Service& operator=(LibEBU128Service&&) = default;
    
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
    // EBU R128 Specific Methods
    // ========================================================================
    
    /// Analysis modes supported by libebur128
    enum class AnalysisMode {
        Momentary = EBUR128_MODE_M,           // Momentary loudness (400ms)
        ShortTerm = EBUR128_MODE_S,           // Short-term loudness (3s)
        Integrated = EBUR128_MODE_I,          // Integrated loudness
        LoudnessRange = EBUR128_MODE_LRA,     // Loudness range
        TruePeak = EBUR128_MODE_TRUE_PEAK,    // True peak detection
        Histogram = EBUR128_MODE_HISTOGRAM    // Histogram for gating
    };
    
    /// Set analysis modes (can combine multiple modes)
    core::VoidResult setAnalysisModes(int modes);
    
    /// Get current analysis modes
    int getAnalysisModes() const;
    
    /// Enable/disable gating (for integrated loudness)
    core::VoidResult setGatingEnabled(bool enabled);
    
    /// Check if gating is enabled
    bool isGatingEnabled() const;
    
    // ========================================================================
    // Real-time Analysis
    // ========================================================================
    
    /// Start real-time analysis session
    core::AsyncResult<core::VoidResult> startRealtimeAnalysis(
        core::SampleRate sampleRate,
        int32_t channels
    );
    
    /// Stop real-time analysis session
    core::AsyncResult<core::VoidResult> stopRealtimeAnalysis();
    
    /// Process real-time audio samples
    core::VoidResult processRealtimeSamples(
        const float* samples,
        size_t frameCount,
        int32_t channels
    );
    
    /// Check if real-time analysis is active
    bool isRealtimeAnalysisActive() const;
    
    // ========================================================================
    // Specific Measurement Methods
    // ========================================================================
    
    /// Get integrated loudness (LUFS)
    core::Result<double> getIntegratedLoudness() const;
    
    /// Get loudness range (LU)
    core::Result<double> getLoudnessRange() const;
    
    /// Get momentary loudness (LUFS)
    core::Result<double> getMomentaryLoudness() const;
    
    /// Get short-term loudness (LUFS)
    core::Result<double> getShortTermLoudness() const;
    
    /// Get true peak values for all channels
    core::Result<std::vector<double>> getTruePeaks() const;
    
    /// Get maximum true peak across all channels
    core::Result<double> getMaxTruePeak() const;
    
    /// Get relative threshold for gating
    core::Result<double> getRelativeThreshold() const;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Set custom channel map
    core::VoidResult setChannelMap(const std::vector<int>& channelMap);
    
    /// Get current channel map
    std::vector<int> getChannelMap() const;
    
    /// Add frames for analysis (batch processing)
    core::VoidResult addFrames(const float* samples, size_t frameCount);
    
    /// Add frames with specific channel layout
    core::VoidResult addFramesWithChannelMap(
        const float* samples, 
        size_t frameCount,
        const std::vector<int>& channelMap
    );
    
    /// Get sample peak values
    core::Result<std::vector<double>> getSamplePeaks() const;
    
    /// Get maximum sample peak
    core::Result<double> getMaxSamplePeak() const;
    
    // ========================================================================
    // Broadcast Standards Compliance
    // ========================================================================
    
    /// Check EBU R128 compliance
    struct ComplianceResult {
        bool isCompliant = false;
        double integratedLoudness = 0.0;  // LUFS
        double loudnessRange = 0.0;       // LU
        double maxTruePeak = 0.0;         // dBTP
        std::string complianceLevel;      // "EBU R128", "ATSC A/85", etc.
        std::vector<std::string> violations;
    };
    
    core::Result<ComplianceResult> checkEBUR128Compliance() const;
    core::Result<ComplianceResult> checkATSCA85Compliance() const;
    
    /// Get target loudness for different standards
    static double getTargetLoudness(const std::string& standard);
    
    /// Get maximum true peak for different standards
    static double getMaxTruePeak(const std::string& standard);
    
    // ========================================================================
    // Export and Reporting
    // ========================================================================
    
    /// Export analysis results to JSON
    core::VoidResult exportToJSON(const std::string& filePath) const;
    
    /// Export analysis results to CSV
    core::VoidResult exportToCSV(const std::string& filePath) const;
    
    /// Generate compliance report
    core::VoidResult generateComplianceReport(
        const std::string& filePath,
        const std::string& standard = "EBU R128"
    ) const;
    
    /// Get analysis summary
    struct AnalysisSummary {
        std::string fileName;
        double duration = 0.0;              // seconds
        core::SampleRate sampleRate = 0;
        int32_t channels = 0;
        double integratedLoudness = 0.0;    // LUFS
        double loudnessRange = 0.0;         // LU
        double maxTruePeak = 0.0;           // dBTP
        double maxSamplePeak = 0.0;         // dBFS
        bool isCompliant = false;
        std::string analysisTime;
    };
    
    AnalysisSummary getAnalysisSummary() const;
    
    // ========================================================================
    // Configuration Constants
    // ========================================================================
    
    static constexpr double SILENCE_THRESHOLD = -70.0;  // dB
    static constexpr double ABSOLUTE_THRESHOLD = -70.0; // LUFS
    static constexpr double RELATIVE_THRESHOLD_OFFSET = -10.0; // LU
    
    // Standard target levels
    static constexpr double EBU_R128_TARGET = -23.0;    // LUFS
    static constexpr double ATSC_A85_TARGET = -24.0;    // LUFS
    static constexpr double STREAMING_TARGET = -16.0;   // LUFS (typical)
    
    // Maximum true peak levels
    static constexpr double EBU_R128_MAX_TP = -1.0;     // dBTP
    static constexpr double ATSC_A85_MAX_TP = -2.0;     // dBTP
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize libebur128 state
    core::VoidResult initializeEBUState(core::SampleRate sampleRate, int32_t channels);
    
    /// Cleanup libebur128 state
    void cleanupEBUState();
    
    /// Update performance metrics
    void updatePerformanceMetrics(double processingTime, bool success);
    
    /// Validate configuration
    core::VoidResult validateConfiguration() const;
    
    /// Convert libebur128 error to our error code
    core::ErrorCode convertEBUError(int ebuError) const;
    
    /// Load audio file for analysis
    core::AsyncResult<core::VoidResult> loadAndAnalyzeAudioFile(const std::string& filePath, core::ProgressCallback progress);
    
private:
    // libebur128 state
    ebur128_state* ebuState_ = nullptr;
    std::mutex ebuStateMutex_;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> isAnalyzing_{false};
    std::atomic<bool> shouldCancel_{false};
    std::atomic<bool> isRealtimeActive_{false};
    
    // Configuration
    std::unordered_map<std::string, std::string> config_;
    std::mutex configMutex_;
    int analysisModes_ = EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_TRUE_PEAK;
    bool gatingEnabled_ = true;
    std::vector<int> channelMap_;
    
    // Analysis results
    mutable std::unordered_map<std::string, double> analysisResults_;
    mutable std::mutex resultsMutex_;
    
    // Performance tracking
    mutable PerformanceMetrics performanceMetrics_;
    mutable std::mutex metricsMutex_;
    
    // Error tracking
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;
    
    // Analysis metadata
    std::string currentFileName_;
    std::chrono::steady_clock::time_point analysisStartTime_;
    core::SampleRate currentSampleRate_ = 0;
    int32_t currentChannels_ = 0;
};

} // namespace mixmind::services