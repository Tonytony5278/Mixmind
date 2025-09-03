# Interface Mapping: Our Abstractions ↔ Tracktion Engine & OSS

## Core Audio Engine Mapping

### Session Management
```cpp
// Our Interface
class ISession {
    virtual std::future<Result> createTrack(const TrackConfig&) = 0;
    virtual std::future<Result> importAudio(const ImportConfig&) = 0;
    virtual std::shared_ptr<ITrack> getTrack(TrackID id) = 0;
    virtual std::future<Result> save(const std::string& path) = 0;
};

// Tracktion Engine Implementation
class TESessionAdapter : public ISession {
private:
    std::unique_ptr<te::Edit> edit;
    te::Engine& engine;
    // Maps to te::Edit operations
};
```

### Transport Control
```cpp
// Our Interface
class ITransport {
    virtual std::future<Result> play() = 0;
    virtual std::future<Result> stop() = 0;
    virtual std::future<Result> record() = 0;
    virtual std::future<Result> locate(TimestampSamples pos) = 0;
    virtual TransportState getState() const = 0;
    virtual TimestampSamples getPosition() const = 0;
};

// Tracktion Engine Mapping
class TETransportAdapter : public ITransport {
private:
    te::TransportControl& transport;
    // Direct 1:1 mapping to TE transport
};
```

### Track Management
```cpp
// Our Interface  
class ITrack {
    virtual TrackID getId() const = 0;
    virtual std::future<Result> insertClip(const ClipConfig&) = 0;
    virtual std::future<Result> insertPlugin(const PluginConfig&) = 0;
    virtual std::shared_ptr<IAutomation> getAutomation(ParamID) = 0;
    virtual float getVolume() const = 0;
    virtual std::future<Result> setVolume(float volume) = 0;
};

// Tracktion Engine Mapping
class TETrackAdapter : public ITrack {
private:
    te::AudioTrack* audioTrack;  // or te::MidiTrack*
    // Maps to TE track operations + plugin chain
};
```

## Plugin System Mapping

### Plugin Host Interface
```cpp
// Our Interface
class IPluginHost {
    virtual std::future<PluginScanResult> scanPlugins() = 0;
    virtual std::future<Result> loadPlugin(PluginID, TrackID, int slot) = 0;
    virtual std::shared_ptr<IPluginInstance> getPlugin(InstanceID) = 0;
};

// Tracktion Engine Mapping
class TEPluginHostAdapter : public IPluginHost {
private:
    te::Engine& engine;
    te::PluginManager& pluginManager;
    // Uses TE's built-in plugin scanning and hosting
    // Leverages JUCE VST3/AU/AAX support
};
```

### Plugin Instance Control
```cpp
// Our Interface
class IPluginInstance {
    virtual std::future<Result> setParameter(ParamID, float value) = 0;
    virtual float getParameter(ParamID) const = 0;
    virtual std::future<Result> loadPreset(const std::string& path) = 0;
    virtual PluginState getState() const = 0;
};

// Tracktion Engine Mapping
class TEPluginInstanceAdapter : public IPluginInstance {
private:
    te::Plugin::Ptr plugin;
    // Direct mapping to TE plugin parameter system
};
```

## Audio Processing Pipeline

### Real-time Audio Processing
```cpp
// Our Interface
class IAudioProcessor {
    virtual void processBlock(AudioBuffer& buffer, MidiBuffer& midi) = 0;
    virtual void prepareToPlay(double sampleRate, int blockSize) = 0;
    virtual void releaseResources() = 0;
};

// Tracktion Engine Integration
// TE handles the processing pipeline internally
// We provide processors that integrate with TE's graph
class TECustomProcessor : public te::Processor {
    // Inherits from TE's processor base class
    // Integrates with TE's audio graph automatically
};
```

## OSS Library Integration

### Loudness Metering (libebur128)
```cpp
// Our Interface
class ILoudnessMeter {
    virtual std::future<LoudnessResult> analyzeLoudness(const AudioSpan&) = 0;
    virtual void startRealTimeMonitoring() = 0;
    virtual LoudnessValues getCurrentValues() const = 0;
};

// libebur128 Implementation
class EBU128LoudnessMeter : public ILoudnessMeter {
private:
    ebur128_state* state;
    // Direct integration with libebur128 C API
    // Thread-safe real-time analysis
};
```

### FFT Analysis (KissFFT)
```cpp
// Our Interface
class IFFT {
    virtual std::future<SpectrumData> computeSpectrum(const AudioSpan&) = 0;
    virtual void setWindowFunction(WindowType type) = 0;
    virtual void setFFTSize(int size) = 0;
};

// KissFFT Implementation  
class KissFFTAnalyzer : public IFFT {
private:
    kiss_fft_cfg cfg;
    // Efficient real-time FFT processing
    // Windowing and overlap-add support
};
```

### Time Stretching
```cpp
// Our Interface
class ITimeStretch {
    virtual std::future<AudioData> stretchAudio(const AudioSpan&, float ratio) = 0;
    virtual void setQuality(StretchQuality quality) = 0;
    virtual void setFormantCorrection(bool enabled) = 0;
};

// Implementation Selection
#ifdef RUBBERBAND_ENABLED
class RubberBandTimeStretch : public ITimeStretch {
private:
    RubberBand::RubberBandStretcher* stretcher;
    // High-quality commercial stretching
};
#endif

class SoundTouchTimeStretch : public ITimeStretch {
private:
    soundtouch::SoundTouch processor;
    // Open-source fallback option
};
```

### Metadata Handling (TagLib)
```cpp
// Our Interface
class IMetadata {
    virtual std::future<MetadataResult> readMetadata(const std::string& path) = 0;
    virtual std::future<Result> writeMetadata(const std::string& path, const Metadata&) = 0;
    virtual SupportedFormats getSupportedFormats() const = 0;
};

// TagLib Implementation
class TagLibMetadata : public IMetadata {
private:
    // Uses TagLib for comprehensive format support
    // Handles ID3, Vorbis Comments, APE, etc.
};
```

### OSC Remote Control
```cpp
// Our Interface
class IOSCRemote {
    virtual std::future<Result> startServer(int port) = 0;
    virtual std::future<Result> sendMessage(const OSCAddress&, const OSCArgs&) = 0;
    virtual void setMessageCallback(OSCCallback callback) = 0;
};

// liblo Implementation
class LibloOSCRemote : public IOSCRemote {
private:
    lo_server server;
    lo_address clients;
    // Full OSC 1.0 compliance
    // Network discovery and auto-configuration
};
```

## AI Integration Bridge

### Action API Schema Validation
```cpp
// Our Interface
class IActionValidator {
    virtual ValidationResult validate(const nlohmann::json& action) = 0;
    virtual SchemaRegistry& getSchemas() = 0;
};

// JSON Schema Validator Implementation
class JSONSchemaActionValidator : public IActionValidator {
private:
    nlohmann::json_schema::json_validator validator;
    // Compile-time and runtime validation
    // Custom error reporting
};
```

### ML Model Runtime (Optional)
```cpp
// Our Interface
class IModelRuntime {
    virtual std::future<InferenceResult> runInference(const ModelInput&) = 0;
    virtual std::future<Result> loadModel(const std::string& path) = 0;
};

#ifdef ONNX_ENABLED
// ONNX Runtime Implementation
class ONNXModelRuntime : public IModelRuntime {
private:
    Ort::Session session;
    Ort::Env environment;
    // On-device ML inference
    // CPU and GPU execution providers
};
#endif
```

## Data Flow Integration

### Session Data Flow
```
Python AI Backend ←→ C++ Action API ←→ Core Interfaces ←→ TE Adapters ←→ Tracktion Engine
                                                    ↓
                               OSS Services ←→ Third-party Libraries
```

### Audio Processing Flow
```
Audio Input → JUCE Audio Devices → TE Audio Engine → Custom Processors → OSS Analysis
                                         ↓                    ↓
                                   Track Processing    Real-time Metering
                                         ↓                    ↓
                                   Plugin Processing   Spectrum Analysis
                                         ↓                    ↓
                                   Mix Bus Processing  Loudness Monitoring
                                         ↓
                                   Audio Output
```

### File I/O Integration
```cpp
// Session Files
te::Edit::save() → .tracktionedit (TE native format)
                ↓
Custom Serializer → .mixproj (our extended format)
                          ↓
                    Metadata Sidecar → project metadata, AI annotations

// Audio Files  
TE Audio Import/Export ←→ TagLib Metadata ←→ File System
                                    ↓
                            Custom Media Database
```

## Threading Model

### Real-time Audio Thread
- **Owner**: Tracktion Engine + JUCE
- **Access**: Direct TE audio processing
- **Restrictions**: No allocations, no blocking calls, lock-free

### UI Thread
- **Owner**: JUCE Application
- **Access**: Interface method calls, async futures
- **Communication**: Message queue to/from audio thread

### AI/Analysis Thread Pool
- **Owner**: Our thread pool manager
- **Access**: OSS libraries (FFT, loudness analysis)
- **Communication**: Async results via futures

### Background I/O Threads
- **Owner**: JUCE + custom I/O manager
- **Access**: File system, network, database
- **Communication**: Async file operations

## Memory Management Strategy

### Audio Buffers
- **Real-time**: JUCE AudioBuffer (stack allocated, reused)
- **Analysis**: Custom buffer pools (pre-allocated)
- **File I/O**: RAII managed buffers

### Plugin State
- **TE Managed**: Plugin parameters, automation
- **Custom**: Extended metadata, AI annotations
- **Serialization**: JSON + binary blob hybrid

### OSS Library Integration
- **Ownership**: RAII wrappers for C libraries
- **Lifetime**: Tied to service object lifetime
- **Error Handling**: Exception safe, resource cleanup guaranteed

This mapping ensures clean separation of concerns while maximizing performance and maintainability.