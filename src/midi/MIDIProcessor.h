#pragma once

#include "MIDIEvent.h"
#include "../core/result.h"
#include <queue>
#include <mutex>
#include <atomic>

namespace mixmind {

// Real-time MIDI processor for instrument tracks
class MIDIProcessor {
public:
    MIDIProcessor();
    ~MIDIProcessor() = default;
    
    // Real-time MIDI event processing
    Result<bool> initialize(double sample_rate, int buffer_size);
    void shutdown();
    
    // MIDI input/output
    void process_midi_input(const MIDIEventBuffer& input_events);
    MIDIEventBuffer get_processed_midi(uint64_t start_sample, uint64_t num_samples);
    
    // Real-time MIDI event injection (for live playing)
    void inject_midi_event(const MIDIEvent& event);
    
    // MIDI processing parameters
    void set_transpose(int semitones) { m_transpose = semitones; }
    int get_transpose() const { return m_transpose; }
    
    void set_velocity_curve(float curve) { m_velocity_curve = curve; }
    float get_velocity_curve() const { return m_velocity_curve; }
    
    void set_channel_filter(int channel) { m_channel_filter = channel; }
    int get_channel_filter() const { return m_channel_filter; } // -1 = all channels
    
    // MIDI timing and quantization
    void set_quantize_enabled(bool enabled) { m_quantize_enabled = enabled; }
    bool is_quantize_enabled() const { return m_quantize_enabled; }
    
    void set_quantize_resolution(int resolution) { m_quantize_resolution = resolution; }
    int get_quantize_resolution() const { return m_quantize_resolution; }
    
    // Performance monitoring
    struct ProcessingStats {
        uint64_t events_processed;
        uint64_t events_dropped;
        double average_latency_ms;
        double peak_latency_ms;
        bool buffer_overrun_detected;
    };
    
    ProcessingStats get_processing_stats() const { return m_stats; }
    void reset_stats();
    
    // MIDI learning and mapping
    void enable_midi_learn(bool enabled) { m_midi_learn_enabled = enabled; }
    bool is_midi_learn_enabled() const { return m_midi_learn_enabled; }
    
    // Get last received MIDI event (for MIDI learn)
    Result<MIDIEvent> get_last_learned_event();
    
private:
    // Real-time processing state
    std::atomic<double> m_sample_rate{44100.0};
    std::atomic<int> m_buffer_size{512};
    std::atomic<bool> m_is_initialized{false};
    
    // MIDI processing parameters
    std::atomic<int> m_transpose{0};           // Semitones (-24 to +24)
    std::atomic<float> m_velocity_curve{1.0f}; // Velocity scaling (0.1 to 2.0)
    std::atomic<int> m_channel_filter{-1};     // -1 = all channels, 0-15 = specific channel
    
    // Quantization settings
    std::atomic<bool> m_quantize_enabled{false};
    std::atomic<int> m_quantize_resolution{16}; // 16th notes by default
    
    // Real-time MIDI buffers (thread-safe)
    std::queue<MIDIEvent> m_input_queue;
    std::queue<MIDIEvent> m_output_queue;
    mutable std::mutex m_queue_mutex;
    
    // Current playing notes (for note-off handling)
    struct PlayingNote {
        uint8_t channel;
        uint8_t note;
        uint8_t velocity;
        uint64_t start_sample;
    };
    std::vector<PlayingNote> m_playing_notes;
    mutable std::mutex m_notes_mutex;
    
    // MIDI learn state
    std::atomic<bool> m_midi_learn_enabled{false};
    MIDIEvent m_last_learned_event{};
    std::atomic<bool> m_has_learned_event{false};
    mutable std::mutex m_learn_mutex;
    
    // Performance statistics
    mutable ProcessingStats m_stats{};
    mutable std::mutex m_stats_mutex;
    
    // Internal processing methods
    MIDIEvent process_single_event(const MIDIEvent& input_event);
    uint8_t apply_velocity_curve(uint8_t original_velocity);
    int apply_transpose(int original_note);
    uint64_t apply_quantization(uint64_t timestamp);
    
    void update_playing_notes(const MIDIEvent& event);
    void handle_all_notes_off();
    
    // Performance monitoring
    void record_processing_latency(double latency_ms);
    void increment_events_processed();
    void increment_events_dropped();
};

} // namespace mixmind