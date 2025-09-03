#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>

// Mock MIDI structures for property testing
namespace mixmind::midi {
    struct MIDINote {
        int pitch = 60;      // C4 (middle C)
        double velocity = 0.8;
        double startTime = 0.0;
        double duration = 1.0;
        
        bool isValid() const {
            return pitch >= 0 && pitch <= 127 &&
                   velocity >= 0.0 && velocity <= 1.0 &&
                   startTime >= 0.0 && duration > 0.0;
        }
    };
    
    class MIDIClip {
        std::vector<MIDINote> notes_;
        
    public:
        void addNote(const MIDINote& note) {
            if (note.isValid()) {
                notes_.push_back(note);
            }
        }
        
        void clear() { notes_.clear(); }
        size_t size() const { return notes_.size(); }
        const std::vector<MIDINote>& getNotes() const { return notes_; }
        
        // Property: notes should be sorted by start time
        bool isChronological() const {
            return std::is_sorted(notes_.begin(), notes_.end(),
                [](const MIDINote& a, const MIDINote& b) {
                    return a.startTime < b.startTime;
                });
        }
        
        // Property: all notes should be within valid MIDI range
        bool allNotesValid() const {
            return std::all_of(notes_.begin(), notes_.end(),
                [](const MIDINote& note) { return note.isValid(); });
        }
        
        void sortByTime() {
            std::sort(notes_.begin(), notes_.end(),
                [](const MIDINote& a, const MIDINote& b) {
                    return a.startTime < b.startTime;
                });
        }
    };
}

using namespace mixmind::midi;

TEST_CASE("MIDI Note Properties", "[midi][properties]") {
    SECTION("Valid note ranges") {
        MIDINote note;
        
        // Property: Valid pitch range [0, 127]
        for (int pitch = 0; pitch <= 127; ++pitch) {
            note.pitch = pitch;
            REQUIRE(note.isValid());
        }
        
        // Invalid pitches should fail validation
        note.pitch = -1;
        REQUIRE_FALSE(note.isValid());
        note.pitch = 128;
        REQUIRE_FALSE(note.isValid());
    }
    
    SECTION("Valid velocity range") {
        MIDINote note;
        
        // Property: Valid velocity range [0.0, 1.0]
        std::vector<double> validVelocities = {0.0, 0.25, 0.5, 0.75, 1.0};
        for (double vel : validVelocities) {
            note.velocity = vel;
            REQUIRE(note.isValid());
        }
        
        // Invalid velocities should fail
        note.velocity = -0.1;
        REQUIRE_FALSE(note.isValid());
        note.velocity = 1.1;
        REQUIRE_FALSE(note.isValid());
    }
    
    SECTION("Time constraints") {
        MIDINote note;
        
        // Property: Start time must be non-negative
        note.startTime = 0.0;
        REQUIRE(note.isValid());
        
        note.startTime = -0.1;
        REQUIRE_FALSE(note.isValid());
        
        // Property: Duration must be positive
        note.startTime = 0.0;
        note.duration = 0.0;
        REQUIRE_FALSE(note.isValid());
        
        note.duration = 0.001;  // Minimal positive duration
        REQUIRE(note.isValid());
    }
}

TEST_CASE("MIDI Clip Properties", "[midi][clip][properties]") {
    SECTION("Only valid notes are added") {
        MIDIClip clip;
        
        // Property: Invalid notes should be rejected
        MIDINote invalidNote;
        invalidNote.pitch = -1;  // Invalid pitch
        clip.addNote(invalidNote);
        
        REQUIRE(clip.size() == 0);  // Should not be added
        
        // Valid note should be added
        MIDINote validNote;
        clip.addNote(validNote);
        REQUIRE(clip.size() == 1);
    }
    
    SECTION("Chronological ordering property") {
        MIDIClip clip;
        
        // Add notes in random time order
        std::vector<double> times = {3.0, 1.0, 4.0, 2.0, 0.5};
        
        for (double time : times) {
            MIDINote note;
            note.startTime = time;
            clip.addNote(note);
        }
        
        // Before sorting, may not be chronological
        // After sorting, must be chronological
        clip.sortByTime();
        REQUIRE(clip.isChronological());
        
        // Verify the actual order
        const auto& notes = clip.getNotes();
        for (size_t i = 1; i < notes.size(); ++i) {
            REQUIRE(notes[i-1].startTime <= notes[i].startTime);
        }
    }
    
    SECTION("All notes valid property") {
        MIDIClip clip;
        
        // Add several valid notes
        for (int i = 0; i < 10; ++i) {
            MIDINote note;
            note.pitch = 60 + (i % 12);  // C4 to B4
            note.velocity = 0.5 + (i * 0.05);
            note.startTime = i * 0.25;
            note.duration = 0.5;
            clip.addNote(note);
        }
        
        REQUIRE(clip.allNotesValid());
        REQUIRE(clip.size() == 10);
    }
    
    SECTION("Empty clip properties") {
        MIDIClip clip;
        
        // Property: Empty clip should satisfy all constraints
        REQUIRE(clip.isChronological());  // Vacuously true
        REQUIRE(clip.allNotesValid());    // Vacuously true
        REQUIRE(clip.size() == 0);
    }
}

TEST_CASE("MIDI Property-based stress tests", "[midi][stress][properties]") {
    SECTION("Large clip maintains properties") {
        MIDIClip clip;
        const int NUM_NOTES = 1000;
        
        // Generate many random valid notes
        for (int i = 0; i < NUM_NOTES; ++i) {
            MIDINote note;
            note.pitch = (i * 7) % 128;           // Pseudo-random pitch
            note.velocity = ((i * 13) % 100) / 100.0;  // Pseudo-random velocity
            note.startTime = (i * 0.1) + ((i * 3) % 10) * 0.01;  // Mixed ordering
            note.duration = 0.1 + ((i * 5) % 20) * 0.05;
            
            clip.addNote(note);
        }
        
        REQUIRE(clip.size() == NUM_NOTES);
        REQUIRE(clip.allNotesValid());
        
        // Sort and verify chronological property
        clip.sortByTime();
        REQUIRE(clip.isChronological());
    }
    
    SECTION("Boundary value analysis") {
        MIDINote note;
        
        // Test exact boundaries
        struct TestCase {
            int pitch;
            double velocity;
            double startTime;
            double duration;
            bool shouldBeValid;
        };
        
        std::vector<TestCase> cases = {
            {0, 0.0, 0.0, 0.001, true},        // All minimum values
            {127, 1.0, 1000.0, 10.0, true},   // All maximum values
            {-1, 0.5, 0.0, 1.0, false},       // Invalid pitch (low)
            {128, 0.5, 0.0, 1.0, false},      // Invalid pitch (high)
            {60, -0.001, 0.0, 1.0, false},    // Invalid velocity (low)
            {60, 1.001, 0.0, 1.0, false},     // Invalid velocity (high)
            {60, 0.5, -0.001, 1.0, false},    // Invalid start time
            {60, 0.5, 0.0, 0.0, false},       // Invalid duration (zero)
            {60, 0.5, 0.0, -0.1, false},      // Invalid duration (negative)
        };
        
        for (const auto& testCase : cases) {
            note.pitch = testCase.pitch;
            note.velocity = testCase.velocity;
            note.startTime = testCase.startTime;
            note.duration = testCase.duration;
            
            if (testCase.shouldBeValid) {
                REQUIRE(note.isValid());
            } else {
                REQUIRE_FALSE(note.isValid());
            }
        }
    }
}