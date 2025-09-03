#include "MIDIProcessor.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace mixmind {

MIDIProcessor::MIDIProcessor() {
    reset_stats();
}

Result<bool> MIDIProcessor::initialize(double sample_rate, int buffer_size) {
    if (sample_rate <= 0 || buffer_size <= 0) {
        return Result<bool>::error("Invalid audio parameters for MIDI processor");
    }
    
    m_sample_rate = sample_rate;
    m_buffer_size = buffer_size;
    
    // Clear any existing state
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        while (!m_input_queue.empty()) m_input_queue.pop();
        while (!m_output_queue.empty()) m_output_queue.pop();
    }
    
    {
        std::lock_guard<std::mutex> lock(m_notes_mutex);
        m_playing_notes.clear();
    }
    
    reset_stats();
    
    m_is_initialized = true;
    return Result<bool>::success(true);
}

void MIDIProcessor::shutdown() {
    m_is_initialized = false;
    
    // Send all notes off to prevent stuck notes
    handle_all_notes_off();
    
    // Clear buffers
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    while (!m_input_queue.empty()) m_input_queue.pop();
    while (!m_output_queue.empty()) m_output_queue.pop();
}

void MIDIProcessor::process_midi_input(const MIDIEventBuffer& input_events) {
    if (!m_is_initialized) return;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    
    for (const auto& event : input_events) {
        // Apply MIDI processing
        MIDIEvent processed_event = process_single_event(event);
        
        // Add to output queue
        m_output_queue.push(processed_event);
        
        // MIDI learn handling
        if (m_midi_learn_enabled) {
            std::lock_guard<std::mutex> learn_lock(m_learn_mutex);
            m_last_learned_event = event;
            m_has_learned_event = true;
        }
        
        increment_events_processed();
    }
    
    // Record processing latency
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    record_processing_latency(duration.count() / 1000.0);
}

MIDIEventBuffer MIDIProcessor::get_processed_midi(uint64_t start_sample, uint64_t num_samples) {
    if (!m_is_initialized) return {};
    
    MIDIEventBuffer result;
    uint64_t end_sample = start_sample + num_samples;
    
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    
    // Extract events within the sample range
    std::queue<MIDIEvent> remaining_events;
    
    while (!m_output_queue.empty()) {
        MIDIEvent event = m_output_queue.front();
        m_output_queue.pop();
        
        if (event.timestamp >= start_sample && event.timestamp < end_sample) {
            // Adjust timestamp relative to buffer start
            event.timestamp -= start_sample;
            result.push_back(event);
        } else if (event.timestamp >= end_sample) {
            // Keep for future processing
            remaining_events.push(event);
        }
        // Events before start_sample are discarded (too late)
    }
    
    // Restore remaining events to queue
    m_output_queue = remaining_events;
    
    // Sort events by timestamp for proper playback order
    sort_midi_events(result);
    
    return result;
}

void MIDIProcessor::inject_midi_event(const MIDIEvent& event) {
    if (!m_is_initialized) return;
    
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    
    MIDIEvent processed_event = process_single_event(event);
    m_output_queue.push(processed_event);
    
    increment_events_processed();
}

MIDIEvent MIDIProcessor::process_single_event(const MIDIEvent& input_event) {
    MIDIEvent processed = input_event;
    
    // Apply channel filtering
    int channel_filter = m_channel_filter;
    if (channel_filter >= 0 && channel_filter <= 15) {
        if (processed.channel != channel_filter) {
            // Filter out this event by making it a dummy event
            processed.type = static_cast<MIDIEventType>(0xFF);
            return processed;
        }
    }
    
    // Process note events
    if (processed.is_note_event()) {
        // Apply transpose
        int transposed_note = apply_transpose(processed.data1);
        if (transposed_note < 0 || transposed_note > 127) {
            // Note out of range, filter it
            processed.type = static_cast<MIDIEventType>(0xFF);
            return processed;
        }
        processed.data1 = static_cast<uint8_t>(transposed_note);
        
        // Apply velocity curve (for note on events)
        if (processed.type == MIDIEventType::NOTE_ON && processed.data2 > 0) {
            processed.data2 = apply_velocity_curve(processed.data2);
        }
        
        // Update playing notes tracking
        update_playing_notes(processed);
    }
    
    // Apply quantization to timing
    if (m_quantize_enabled) {
        processed.timestamp = apply_quantization(processed.timestamp);
    }
    
    return processed;
}

uint8_t MIDIProcessor::apply_velocity_curve(uint8_t original_velocity) {
    if (original_velocity == 0) return 0;
    
    float curve = m_velocity_curve;
    float normalized = original_velocity / 127.0f;
    
    // Apply curve (power function)
    float curved = std::pow(normalized, 1.0f / curve);
    
    // Convert back to MIDI range
    int result = static_cast<int>(curved * 127.0f + 0.5f);
    return static_cast<uint8_t>(std::clamp(result, 1, 127));
}

int MIDIProcessor::apply_transpose(int original_note) {
    return original_note + m_transpose;
}

uint64_t MIDIProcessor::apply_quantization(uint64_t timestamp) {
    double sample_rate = m_sample_rate;
    int resolution = m_quantize_resolution;
    
    // Calculate samples per quantization unit
    // Assuming 120 BPM, 4/4 time signature
    double beats_per_second = 120.0 / 60.0;
    double samples_per_beat = sample_rate / beats_per_second;
    double samples_per_unit = samples_per_beat * (4.0 / resolution); // 4 = quarter notes per bar
    
    // Quantize to nearest unit
    uint64_t quantized = static_cast<uint64_t>((timestamp / samples_per_unit + 0.5) * samples_per_unit);
    
    return quantized;
}

void MIDIProcessor::update_playing_notes(const MIDIEvent& event) {
    std::lock_guard<std::mutex> lock(m_notes_mutex);
    
    if (event.type == MIDIEventType::NOTE_ON && event.data2 > 0) {
        // Add playing note
        PlayingNote note;
        note.channel = event.channel;
        note.note = event.data1;
        note.velocity = event.data2;
        note.start_sample = event.timestamp;
        
        m_playing_notes.push_back(note);
    }
    else if (event.type == MIDIEventType::NOTE_OFF || 
             (event.type == MIDIEventType::NOTE_ON && event.data2 == 0)) {
        // Remove playing note
        m_playing_notes.erase(
            std::remove_if(m_playing_notes.begin(), m_playing_notes.end(),
                [&](const PlayingNote& note) {
                    return note.channel == event.channel && note.note == event.data1;
                }),
            m_playing_notes.end());
    }
}

void MIDIProcessor::handle_all_notes_off() {
    std::lock_guard<std::mutex> lock(m_notes_mutex);
    
    // Generate note-off events for all playing notes
    std::lock_guard<std::mutex> queue_lock(m_queue_mutex);
    
    for (const auto& note : m_playing_notes) {
        MIDIEvent note_off = MIDIEvent::note_off(note.channel, note.note, 64, 0);
        m_output_queue.push(note_off);
    }
    
    m_playing_notes.clear();
}

Result<MIDIEvent> MIDIProcessor::get_last_learned_event() {
    std::lock_guard<std::mutex> lock(m_learn_mutex);
    
    if (!m_has_learned_event) {
        return Result<MIDIEvent>::error("No MIDI event learned yet");
    }
    
    return Result<MIDIEvent>::success(m_last_learned_event);
}

void MIDIProcessor::reset_stats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_stats.events_processed = 0;
    m_stats.events_dropped = 0;
    m_stats.average_latency_ms = 0.0;
    m_stats.peak_latency_ms = 0.0;
    m_stats.buffer_overrun_detected = false;
}

void MIDIProcessor::record_processing_latency(double latency_ms) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    // Update peak latency
    if (latency_ms > m_stats.peak_latency_ms) {
        m_stats.peak_latency_ms = latency_ms;
    }
    
    // Update rolling average (simple exponential moving average)
    const double alpha = 0.1;
    m_stats.average_latency_ms = alpha * latency_ms + (1.0 - alpha) * m_stats.average_latency_ms;
    
    // Check for buffer overrun (>10ms is concerning for real-time)
    if (latency_ms > 10.0) {
        m_stats.buffer_overrun_detected = true;
    }
}

void MIDIProcessor::increment_events_processed() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats.events_processed++;
}

void MIDIProcessor::increment_events_dropped() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats.events_dropped++;
}

} // namespace mixmind