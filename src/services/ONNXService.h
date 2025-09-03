#pragma once

#include "IOSSService.h"
#include "../core/types.h"
#include "../core/result.h"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace mixmind::services {

// ============================================================================
// ONNX Service - Machine Learning inference using ONNX Runtime
// ============================================================================

class ONNXService : public IOSSService {
public:
    ONNXService();
    ~ONNXService() override;
    
    // Non-copyable, movable
    ONNXService(const ONNXService&) = delete;
    ONNXService& operator=(const ONNXService&) = delete;
    ONNXService(ONNXService&&) = default;
    ONNXService& operator=(ONNXService&&) = default;
    
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
    // ONNX Model Management
    // ========================================================================
    
    /// Tensor data types
    enum class DataType {
        Float16,
        Float32,
        Float64,
        Int8,
        Int16,
        Int32,
        Int64,
        UInt8,
        UInt16,
        UInt32,
        UInt64,
        Bool,
        String
    };
    
    /// Tensor shape and metadata
    struct TensorInfo {
        std::string name;
        DataType dataType;
        std::vector<int64_t> shape;
        std::vector<int64_t> maxShape;  // For dynamic shapes
        size_t totalElements = 0;
        size_t sizeBytes = 0;
        bool isDynamic = false;
    };
    
    /// Model metadata
    struct ModelInfo {
        std::string modelPath;
        std::string modelName;
        std::string description;
        std::string version;
        std::string domain;
        std::string producer;
        std::vector<TensorInfo> inputs;
        std::vector<TensorInfo> outputs;
        size_t modelSizeBytes = 0;
        std::unordered_map<std::string, std::string> metadata;
    };
    
    /// Load ONNX model from file
    core::AsyncResult<core::VoidResult> loadModel(
        const std::string& modelPath,
        const std::string& modelName = ""
    );
    
    /// Load model from memory buffer
    core::AsyncResult<core::VoidResult> loadModelFromMemory(
        const std::vector<uint8_t>& modelData,
        const std::string& modelName
    );
    
    /// Unload model
    core::VoidResult unloadModel(const std::string& modelName);
    
    /// Get loaded models
    std::vector<std::string> getLoadedModels() const;
    
    /// Get model information
    std::optional<ModelInfo> getModelInfo(const std::string& modelName) const;
    
    /// Check if model is loaded
    bool isModelLoaded(const std::string& modelName) const;
    
    // ========================================================================
    // Tensor Operations
    // ========================================================================
    
    /// Generic tensor data container
    struct Tensor {
        std::string name;
        DataType dataType;
        std::vector<int64_t> shape;
        std::vector<uint8_t> data;
        
        Tensor() = default;
        
        /// Create tensor from float data
        static Tensor fromFloatVector(
            const std::string& name,
            const std::vector<float>& data,
            const std::vector<int64_t>& shape
        );
        
        /// Create tensor from audio buffer
        static Tensor fromAudioBuffer(
            const std::string& name,
            const core::FloatAudioBuffer& buffer
        );
        
        /// Extract float data
        std::vector<float> toFloatVector() const;
        
        /// Extract audio buffer
        core::FloatAudioBuffer toAudioBuffer() const;
        
        /// Get element count
        size_t getElementCount() const;
        
        /// Get size in bytes
        size_t getSizeBytes() const;
        
        /// Validate shape consistency
        bool isShapeValid() const;
    };
    
    /// Create empty tensor with specified shape
    Tensor createTensor(
        const std::string& name,
        DataType dataType,
        const std::vector<int64_t>& shape
    );
    
    /// Reshape tensor
    core::VoidResult reshapeTensor(
        Tensor& tensor,
        const std::vector<int64_t>& newShape
    );
    
    /// Convert tensor data type
    core::Result<Tensor> convertTensorType(
        const Tensor& tensor,
        DataType targetType
    );
    
    // ========================================================================
    // Model Inference
    // ========================================================================
    
    /// Run inference on a model
    core::AsyncResult<core::Result<std::vector<Tensor>>> runInference(
        const std::string& modelName,
        const std::vector<Tensor>& inputs
    );
    
    /// Run inference with input/output name mapping
    core::AsyncResult<core::Result<std::unordered_map<std::string, Tensor>>> runInferenceNamed(
        const std::string& modelName,
        const std::unordered_map<std::string, Tensor>& namedInputs
    );
    
    /// Batch inference for multiple input sets
    core::AsyncResult<core::Result<std::vector<std::vector<Tensor>>>> runBatchInference(
        const std::string& modelName,
        const std::vector<std::vector<Tensor>>& batchInputs,
        core::ProgressCallback progress = nullptr
    );
    
    // ========================================================================
    // Audio-Specific ML Operations
    // ========================================================================
    
    /// Audio analysis models
    enum class AudioAnalysisModel {
        BeatTracker,            // Beat and tempo detection
        ChordRecognition,       // Chord progression analysis
        KeyDetection,          // Musical key detection
        GenreClassification,   // Music genre classification
        InstrumentRecognition, // Instrument identification
        SpeechDetection,       // Speech vs music classification
        NoiseReduction,        // Noise suppression
        SourceSeparation,      // Vocal/instrument separation
        AudioUpsampling,       // Super-resolution upsampling
        AudioDenoising,        // Deep noise reduction
        VoiceConversion,       // Voice style transfer
        MusicGeneration        // AI music generation
    };
    
    /// Audio analysis result
    struct AudioAnalysisResult {
        AudioAnalysisModel modelType;
        std::string modelName;
        double confidence = 0.0;
        std::unordered_map<std::string, float> values;      // Numeric results
        std::unordered_map<std::string, std::string> labels; // Text results
        std::vector<float> timeSeriesData;                  // Time-based data
        core::FloatAudioBuffer processedAudio;              // Processed audio output
        std::string analysisDetails;
    };
    
    /// Analyze audio with specific model
    core::AsyncResult<core::Result<AudioAnalysisResult>> analyzeAudio(
        AudioAnalysisModel modelType,
        const core::FloatAudioBuffer& audioBuffer,
        core::SampleRate sampleRate,
        const std::string& modelName = ""
    );
    
    /// Process audio with ML model (e.g., noise reduction, source separation)
    core::AsyncResult<core::Result<core::FloatAudioBuffer>> processAudio(
        AudioAnalysisModel modelType,
        const core::FloatAudioBuffer& audioBuffer,
        core::SampleRate sampleRate,
        const std::string& modelName = ""
    );
    
    /// Real-time audio processing setup
    core::AsyncResult<core::VoidResult> setupRealtimeProcessing(
        AudioAnalysisModel modelType,
        core::SampleRate sampleRate,
        int32_t frameSize,
        const std::string& modelName = ""
    );
    
    /// Process real-time audio frame
    core::Result<core::FloatAudioBuffer> processRealtimeFrame(
        const core::FloatAudioBuffer& inputFrame
    );
    
    /// Stop real-time processing
    core::VoidResult stopRealtimeProcessing();
    
    // ========================================================================
    // Execution Providers and Hardware Acceleration
    // ========================================================================
    
    /// Available execution providers
    enum class ExecutionProvider {
        CPU,            // Default CPU execution
        CUDA,          // NVIDIA GPU acceleration
        DirectML,      // Microsoft DirectML (Windows)
        OpenVINO,      // Intel OpenVINO
        TensorRT,      // NVIDIA TensorRT
        CoreML,        // Apple CoreML (macOS)
        NNAPI,         // Android NNAPI
        ROCm           // AMD ROCm
    };
    
    /// Set preferred execution provider
    core::VoidResult setExecutionProvider(ExecutionProvider provider);
    
    /// Get current execution provider
    ExecutionProvider getExecutionProvider() const;
    
    /// Get available execution providers
    std::vector<ExecutionProvider> getAvailableProviders() const;
    
    /// GPU memory management
    core::VoidResult setGPUMemoryLimit(size_t memoryLimitMB);
    size_t getGPUMemoryLimit() const;
    size_t getGPUMemoryUsage() const;
    
    // ========================================================================
    // Model Optimization
    // ========================================================================
    
    /// Optimization levels
    enum class OptimizationLevel {
        DisableAll,     // No optimizations
        Basic,          // Basic optimizations
        Extended,       // Extended optimizations
        All             // All optimizations
    };
    
    /// Set optimization level
    core::VoidResult setOptimizationLevel(OptimizationLevel level);
    
    /// Get optimization level
    OptimizationLevel getOptimizationLevel() const;
    
    /// Enable/disable parallel execution
    core::VoidResult setParallelExecutionEnabled(bool enabled);
    
    /// Set number of threads for CPU execution
    core::VoidResult setThreadCount(int32_t threadCount);
    
    /// Get thread count
    int32_t getThreadCount() const;
    
    // ========================================================================
    // Model Repository and Download
    // ========================================================================
    
    /// Built-in model information
    struct BuiltInModel {
        std::string name;
        std::string description;
        AudioAnalysisModel modelType;
        std::string downloadUrl;
        std::string localPath;
        size_t modelSizeMB = 0;
        std::string version;
        bool isDownloaded = false;
        bool isLoaded = false;
    };
    
    /// Get available built-in models
    std::vector<BuiltInModel> getBuiltInModels() const;
    
    /// Download built-in model
    core::AsyncResult<core::VoidResult> downloadBuiltInModel(
        const std::string& modelName,
        core::ProgressCallback progress = nullptr
    );
    
    /// Check for model updates
    core::AsyncResult<core::Result<std::vector<std::string>>> checkForModelUpdates();
    
    /// Set models directory
    core::VoidResult setModelsDirectory(const std::string& directory);
    
    /// Get models directory
    std::string getModelsDirectory() const;
    
    // ========================================================================
    // Performance Profiling
    // ========================================================================
    
    struct InferenceProfile {
        std::string modelName;
        double preprocessingTime = 0.0;    // milliseconds
        double inferenceTime = 0.0;        // milliseconds
        double postprocessingTime = 0.0;   // milliseconds
        double totalTime = 0.0;             // milliseconds
        size_t inputSizeBytes = 0;
        size_t outputSizeBytes = 0;
        size_t memoryUsageBytes = 0;
        int32_t batchSize = 1;
        std::string executionProvider;
    };
    
    /// Enable/disable profiling
    core::VoidResult setProfilingEnabled(bool enabled);
    
    /// Get profiling data for last inference
    InferenceProfile getLastInferenceProfile() const;
    
    /// Get average profiling data
    InferenceProfile getAverageInferenceProfile(const std::string& modelName) const;
    
    /// Clear profiling data
    void clearProfilingData();
    
    // ========================================================================
    // Model Validation and Testing
    // ========================================================================
    
    /// Validate model integrity
    core::AsyncResult<core::VoidResult> validateModel(const std::string& modelName);
    
    /// Test model with sample data
    core::AsyncResult<core::Result<bool>> testModel(
        const std::string& modelName,
        const std::vector<Tensor>& testInputs,
        const std::vector<Tensor>& expectedOutputs,
        float tolerance = 1e-6f
    );
    
    /// Benchmark model performance
    core::AsyncResult<core::Result<InferenceProfile>> benchmarkModel(
        const std::string& modelName,
        const std::vector<int64_t>& inputShape,
        int32_t iterations = 100
    );
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Initialize ONNX Runtime session
    core::VoidResult initializeSession(
        const std::string& modelPath,
        const std::string& modelName
    );
    
    /// Create ONNX Runtime session options
    std::unique_ptr<Ort::SessionOptions> createSessionOptions() const;
    
    /// Convert our tensor to ONNX Value
    Ort::Value tensorToOrtValue(const Tensor& tensor) const;
    
    /// Convert ONNX Value to our tensor
    Tensor ortValueToTensor(const Ort::Value& ortValue, const std::string& name) const;
    
    /// Get data type size in bytes
    size_t getDataTypeSize(DataType dataType) const;
    
    /// Convert ONNX data type to our enum
    DataType convertONNXDataType(ONNXTensorElementDataType onnxType) const;
    
    /// Convert our data type to ONNX
    ONNXTensorElementDataType convertToONNXDataType(DataType dataType) const;
    
    /// Preprocess audio for model input
    core::Result<Tensor> preprocessAudioForModel(
        const core::FloatAudioBuffer& audioBuffer,
        core::SampleRate sampleRate,
        AudioAnalysisModel modelType
    ) const;
    
    /// Postprocess model output for audio
    core::Result<AudioAnalysisResult> postprocessModelOutput(
        const std::vector<Tensor>& outputs,
        AudioAnalysisModel modelType
    ) const;
    
    /// Update performance metrics
    void updatePerformanceMetrics(double processingTime, bool success);
    
private:
    // ONNX Runtime environment and sessions
    std::unique_ptr<Ort::Env> env_;
    std::unordered_map<std::string, std::unique_ptr<Ort::Session>> sessions_;
    std::unordered_map<std::string, ModelInfo> modelInfos_;
    std::mutex sessionsMutex_;
    
    // Service state
    std::atomic<bool> isInitialized_{false};
    
    // Configuration
    std::unordered_map<std::string, std::string> config_;
    std::mutex configMutex_;
    
    // Execution settings
    ExecutionProvider executionProvider_ = ExecutionProvider::CPU;
    OptimizationLevel optimizationLevel_ = OptimizationLevel::Extended;
    int32_t threadCount_ = 0;  // 0 = auto
    size_t gpuMemoryLimit_ = 1024; // MB
    bool parallelExecution_ = true;
    
    // Models directory and built-in models
    std::string modelsDirectory_;
    std::vector<BuiltInModel> builtInModels_;
    
    // Real-time processing state
    std::atomic<bool> isRealtimeProcessingActive_{false};
    std::string realtimeModelName_;
    AudioAnalysisModel realtimeModelType_;
    
    // Profiling
    std::atomic<bool> profilingEnabled_{false};
    mutable InferenceProfile lastProfile_;
    mutable std::unordered_map<std::string, std::vector<InferenceProfile>> profileHistory_;
    mutable std::mutex profilingMutex_;
    
    // Performance tracking
    mutable PerformanceMetrics performanceMetrics_;
    mutable std::mutex metricsMutex_;
    
    // Error tracking
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;
};

} // namespace mixmind::services