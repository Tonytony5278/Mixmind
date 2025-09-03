#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <map>

// Mock MIDI processing structures for benchmarking
namespace mixmind::midi {
    
    struct MIDIEvent {
        uint32_t timestamp;  // In samples
        uint8_t status;      // MIDI status byte
        uint8_t data1;       // First data byte (note/CC number)
        uint8_t data2;       // Second data byte (velocity/value)
        
        bool operator<(const MIDIEvent& other) const {
            return timestamp < other.timestamp;
        }
        
        bool isNoteOn() const { return (status & 0xF0) == 0x90 && data2 > 0; }
        bool isNoteOff() const { return (status & 0xF0) == 0x80 || ((status & 0xF0) == 0x90 && data2 == 0); }
        bool isCC() const { return (status & 0xF0) == 0xB0; }
        
        uint8_t getChannel() const { return status & 0x0F; }
        uint8_t getNote() const { return data1; }
        uint8_t getVelocity() const { return data2; }
        uint8_t getCCNumber() const { return data1; }
        uint8_t getCCValue() const { return data2; }
    };
    
    class MIDISequence {
        std::vector<MIDIEvent> events_;
        
    public:
        void addEvent(const MIDIEvent& event) {
            events_.push_back(event);
        }
        
        void sortByTime() {
            std::sort(events_.begin(), events_.end());
        }
        
        // Get events in a time range
        std::vector<MIDIEvent> getEventsInRange(uint32_t start, uint32_t end) const {
            std::vector<MIDIEvent> result;
            for (const auto& event : events_) {
                if (event.timestamp >= start && event.timestamp < end) {
                    result.push_back(event);
                }
            }
            return result;
        }
        
        // Quantize events to grid
        void quantize(uint32_t grid_size) {
            for (auto& event : events_) {
                uint32_t grid_pos = ((event.timestamp + grid_size / 2) / grid_size) * grid_size;
                event.timestamp = grid_pos;
            }
        }
        
        // Transpose all note events
        void transpose(int semitones) {
            for (auto& event : events_) {
                if (event.isNoteOn() || event.isNoteOff()) {
                    int newNote = static_cast<int>(event.data1) + semitones;
                    event.data1 = static_cast<uint8_t>(std::clamp(newNote, 0, 127));
                }
            }
        }
        
        // Scale velocities
        void scaleVelocities(float factor) {
            for (auto& event : events_) {
                if (event.isNoteOn()) {
                    int newVel = static_cast<int>(event.data2 * factor);
                    event.data2 = static_cast<uint8_t>(std::clamp(newVel, 1, 127));
                }
            }
        }
        
        size_t size() const { return events_.size(); }
        const std::vector<MIDIEvent>& getEvents() const { return events_; }
        void clear() { events_.clear(); }
    };
    
    // MIDI note tracking for polyphonic processing
    class NoteTracker {
        std::map<uint8_t, uint32_t> active_notes_;  // note -> timestamp
        
    public:
        void noteOn(uint8_t note, uint32_t timestamp) {
            active_notes_[note] = timestamp;
        }
        
        void noteOff(uint8_t note) {
            active_notes_.erase(note);
        }
        
        bool isNoteActive(uint8_t note) const {
            return active_notes_.find(note) != active_notes_.end();
        }
        
        uint32_t getNoteOnTime(uint8_t note) const {
            auto it = active_notes_.find(note);
            return it != active_notes_.end() ? it->second : 0;
        }
        
        std::vector<uint8_t> getActiveNotes() const {
            std::vector<uint8_t> notes;
            for (const auto& pair : active_notes_) {
                notes.push_back(pair.first);
            }
            return notes;
        }
        
        size_t getActiveCount() const { return active_notes_.size(); }
        void clear() { active_notes_.clear(); }
    };
    
    // Generate test MIDI data
    MIDISequence generateRandomMIDI(size_t num_events, uint32_t duration_samples) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        std::uniform_int_distribution<uint32_t> time_dist(0, duration_samples);
        std::uniform_int_distribution<uint8_t> note_dist(36, 84);  // C2 to C6
        std::uniform_int_distribution<uint8_t> vel_dist(40, 127);
        std::uniform_int_distribution<int> type_dist(0, 1);  // 0=note on, 1=note off
        
        MIDISequence sequence;
        
        for (size_t i = 0; i < num_events; ++i) {
            MIDIEvent event;
            event.timestamp = time_dist(gen);
            event.data1 = note_dist(gen);
            
            if (type_dist(gen) == 0) {
                event.status = 0x90;  // Note On, channel 0
                event.data2 = vel_dist(gen);
            } else {
                event.status = 0x80;  // Note Off, channel 0
                event.data2 = 0;
            }
            
            sequence.addEvent(event);
        }
        
        return sequence;
    }
    
    MIDISequence generateScale(uint8_t root_note, uint32_t samples_per_note) {
        MIDISequence sequence;
        
        // Major scale intervals
        std::vector<int> intervals = {0, 2, 4, 5, 7, 9, 11, 12};
        
        for (size_t i = 0; i < intervals.size(); ++i) {
            // Note On
            MIDIEvent noteOn;
            noteOn.timestamp = i * samples_per_note;
            noteOn.status = 0x90;
            noteOn.data1 = root_note + intervals[i];
            noteOn.data2 = 80;
            sequence.addEvent(noteOn);
            
            // Note Off
            MIDIEvent noteOff;
            noteOff.timestamp = i * samples_per_note + samples_per_note - 100;
            noteOff.status = 0x80;
            noteOff.data1 = root_note + intervals[i];
            noteOff.data2 = 0;
            sequence.addEvent(noteOff);
        }
        
        return sequence;
    }
}

using namespace mixmind::midi;

TEST_CASE("MIDI Processing Benchmarks", "[benchmark][midi]") {
    
    // Standard test data sizes
    constexpr size_t SMALL_SEQ = 100;     // Small MIDI sequence
    constexpr size_t MEDIUM_SEQ = 1000;   // Typical song length
    constexpr size_t LARGE_SEQ = 10000;   // Dense sequence
    constexpr uint32_t SAMPLE_DURATION = 44100 * 10;  // 10 seconds at 44.1kHz
    
    SECTION("MIDI sequence creation and sorting") {
        BENCHMARK("Generate small random MIDI sequence") {
            auto sequence = generateRandomMIDI(SMALL_SEQ, SAMPLE_DURATION);
            sequence.sortByTime();
            return sequence.size();
        };
        
        BENCHMARK("Generate medium random MIDI sequence") {
            auto sequence = generateRandomMIDI(MEDIUM_SEQ, SAMPLE_DURATION);
            sequence.sortByTime();
            return sequence.size();
        };
        
        BENCHMARK("Generate large random MIDI sequence") {
            auto sequence = generateRandomMIDI(LARGE_SEQ, SAMPLE_DURATION);
            sequence.sortByTime();
            return sequence.size();
        };
        
        BENCHMARK("Sort pre-existing medium sequence") {
            static auto sequence = generateRandomMIDI(MEDIUM_SEQ, SAMPLE_DURATION);
            auto copy = sequence;  // Copy for sorting
            copy.sortByTime();
            return copy.size();
        };
    }
    
    SECTION("MIDI event filtering and range queries") {
        auto test_sequence = generateRandomMIDI(MEDIUM_SEQ, SAMPLE_DURATION);
        test_sequence.sortByTime();
        
        BENCHMARK("Get events in time range - 10% of sequence") {
            uint32_t start = SAMPLE_DURATION * 0.45f;
            uint32_t end = SAMPLE_DURATION * 0.55f;
            auto events = test_sequence.getEventsInRange(start, end);
            return events.size();
        };
        
        BENCHMARK("Get events in time range - 50% of sequence") {
            uint32_t start = SAMPLE_DURATION * 0.25f;
            uint32_t end = SAMPLE_DURATION * 0.75f;
            auto events = test_sequence.getEventsInRange(start, end);
            return events.size();
        };
        
        BENCHMARK("Count note events vs CC events") {
            size_t note_count = 0, cc_count = 0;
            for (const auto& event : test_sequence.getEvents()) {
                if (event.isNoteOn() || event.isNoteOff()) {
                    note_count++;
                } else if (event.isCC()) {
                    cc_count++;
                }
            }
            return note_count + cc_count;
        };
    }
    
    SECTION("MIDI quantization performance") {
        auto test_sequence = generateRandomMIDI(MEDIUM_SEQ, SAMPLE_DURATION);
        
        BENCHMARK("Quantize to 16th notes") {
            auto sequence = test_sequence;  // Copy
            uint32_t sixteenth_note = 44100 / 4;  // Assuming 120 BPM
            sequence.quantize(sixteenth_note);
            return sequence.size();
        };
        
        BENCHMARK("Quantize to 8th notes") {
            auto sequence = test_sequence;  // Copy
            uint32_t eighth_note = 44100 / 2;
            sequence.quantize(eighth_note);
            return sequence.size();
        };
        
        BENCHMARK("Quantize to quarter notes") {
            auto sequence = test_sequence;  // Copy
            uint32_t quarter_note = 44100;
            sequence.quantize(quarter_note);
            return sequence.size();
        };
    }
    
    SECTION("MIDI transformation benchmarks") {
        auto test_sequence = generateScale(60, 44100 / 2);  // C4 scale, 8th notes
        
        BENCHMARK("Transpose sequence +7 semitones") {
            auto sequence = test_sequence;  // Copy
            sequence.transpose(7);
            return sequence.size();
        };
        
        BENCHMARK("Transpose sequence -12 semitones") {
            auto sequence = test_sequence;  // Copy
            sequence.transpose(-12);
            return sequence.size();
        };
        
        BENCHMARK("Scale velocities by 0.8") {
            auto sequence = test_sequence;  // Copy
            sequence.scaleVelocities(0.8f);
            return sequence.size();
        };
        
        BENCHMARK("Scale velocities by 1.25") {
            auto sequence = test_sequence;  // Copy
            sequence.scaleVelocities(1.25f);
            return sequence.size();
        };
    }
    
    SECTION("Note tracking and polyphony") {
        auto sequence = generateRandomMIDI(MEDIUM_SEQ, SAMPLE_DURATION);
        sequence.sortByTime();
        
        BENCHMARK("Track polyphonic note events") {
            NoteTracker tracker;
            size_t max_polyphony = 0;
            
            for (const auto& event : sequence.getEvents()) {
                if (event.isNoteOn()) {
                    tracker.noteOn(event.getNote(), event.timestamp);
                } else if (event.isNoteOff()) {
                    tracker.noteOff(event.getNote());
                }
                
                max_polyphony = std::max(max_polyphony, tracker.getActiveCount());
            }
            
            return max_polyphony;
        };
        
        BENCHMARK("Query active notes frequently") {
            NoteTracker tracker;
            size_t query_count = 0;
            
            for (const auto& event : sequence.getEvents()) {
                if (event.isNoteOn()) {
                    tracker.noteOn(event.getNote(), event.timestamp);
                } else if (event.isNoteOff()) {
                    tracker.noteOff(event.getNote());
                }
                
                // Query every 10th event
                if ((query_count % 10) == 0) {
                    auto active = tracker.getActiveNotes();
                    query_count += active.size();
                }
                query_count++;
            }
            
            return query_count;
        };
    }
}

TEST_CASE("Real-time MIDI Performance", "[benchmark][midi][realtime]") {
    
    SECTION("Real-time MIDI event processing") {
        constexpr size_t BLOCK_SIZE = 128;
        constexpr double SAMPLE_RATE = 44100.0;
        constexpr uint32_t EVENTS_PER_BLOCK = 10;  // Dense MIDI
        
        auto generateBlockEvents = [](uint32_t block_start) {
            std::vector<MIDIEvent> events;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> offset_dist(0, BLOCK_SIZE - 1);
            std::uniform_int_distribution<uint8_t> note_dist(60, 72);
            
            for (size_t i = 0; i < EVENTS_PER_BLOCK; ++i) {
                MIDIEvent event;
                event.timestamp = block_start + offset_dist(gen);
                event.status = 0x90;  // Note On
                event.data1 = note_dist(gen);
                event.data2 = 80;
                events.push_back(event);
            }
            
            return events;
        };
        
        BENCHMARK("Process real-time MIDI block") {
            static uint32_t block_counter = 0;
            uint32_t block_start = block_counter * BLOCK_SIZE;
            
            auto events = generateBlockEvents(block_start);
            
            // Simulate real-time processing
            NoteTracker tracker;
            size_t processed = 0;
            
            for (const auto& event : events) {
                if (event.isNoteOn()) {
                    tracker.noteOn(event.getNote(), event.timestamp);
                    processed++;
                }
            }
            
            block_counter++;
            return processed;
        };
    }
    
    SECTION("MIDI latency simulation") {
        BENCHMARK("Low-latency MIDI processing chain") {
            // Simulate a complete MIDI processing chain with minimal latency
            
            // Input: MIDI event
            MIDIEvent input_event;
            input_event.timestamp = 0;
            input_event.status = 0x90;
            input_event.data1 = 69;  // A4
            input_event.data2 = 100;
            
            // Stage 1: Event validation (should be near-instant)
            bool valid = input_event.isNoteOn() && 
                        input_event.data1 <= 127 && 
                        input_event.data2 > 0;
            
            if (!valid) return 0;
            
            // Stage 2: Channel routing (map-based lookup)
            std::map<uint8_t, uint8_t> channel_map = {{0, 1}, {1, 2}, {2, 3}};
            uint8_t output_channel = channel_map[input_event.getChannel()];
            
            // Stage 3: Note transformation (transpose + velocity scale)
            MIDIEvent output_event = input_event;
            int new_note = static_cast<int>(output_event.data1) + 7;  // +7 semitones
            output_event.data1 = static_cast<uint8_t>(std::clamp(new_note, 0, 127));
            output_event.data2 = static_cast<uint8_t>(output_event.data2 * 0.8f);  // Reduce velocity
            
            return output_event.data1;
        };
    }
    
    SECTION("Memory allocation patterns") {
        BENCHMARK("Vector-based event storage") {
            std::vector<MIDIEvent> events;
            events.reserve(1000);  // Pre-allocate
            
            for (int i = 0; i < 100; ++i) {
                MIDIEvent event;
                event.timestamp = i * 441;  // 10ms intervals
                event.status = 0x90;
                event.data1 = 60 + (i % 12);
                event.data2 = 80;
                events.push_back(event);
            }
            
            return events.size();
        };
        
        BENCHMARK("Map-based note tracking") {
            std::map<uint8_t, uint32_t> note_states;
            
            for (int i = 0; i < 100; ++i) {
                uint8_t note = 60 + (i % 24);
                if ((i % 2) == 0) {
                    note_states[note] = i * 441;  // Note on
                } else {
                    note_states.erase(note);      // Note off
                }
            }
            
            return note_states.size();
        };
        
        BENCHMARK("Circular buffer for real-time events") {
            constexpr size_t BUFFER_SIZE = 256;
            std::array<MIDIEvent, BUFFER_SIZE> circular_buffer;
            size_t write_pos = 0;
            
            for (int i = 0; i < 1000; ++i) {
                MIDIEvent& event = circular_buffer[write_pos];
                event.timestamp = i;
                event.status = 0x90;
                event.data1 = 60;
                event.data2 = 80;
                
                write_pos = (write_pos + 1) % BUFFER_SIZE;
            }
            
            return write_pos;
        };
    }
}