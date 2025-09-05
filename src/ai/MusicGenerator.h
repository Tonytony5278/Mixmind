#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include "StyleTransfer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>

namespace mixmind::ai {

// ============================================================================
// Music Generation Types
// ============================================================================

struct MelodyNote {
    std::string pitch;      // "C", "D#", "Bb", etc.
    int octave = 4;         // MIDI octave (4 = middle C octave)
    int duration = 4;       // Duration in 16th note units (4 = quarter note)
    int velocity = 64;      // MIDI velocity (0-127)
    int startTime = 0;      // Start time in 16th note units
    bool isRest = false;    // True if this is a rest
    
    // Musical expression
    bool isAccented = false;
    bool isStaccato = false;
    bool isLegato = false;
    float pitchBend = 0.0f; // Pitch bend amount
};

struct MelodyLine {
    std::string name;               // "Main Melody", "Bass Line", etc.
    std::string instrument;         // Instrument name
    int octave = 4;                // Base octave
    std::vector<MelodyNote> notes;  // Sequence of notes
    
    // AI enhancements
    std::string aiEnhancements;     // AI suggestions for this melody
    float musicalityScore = 0.5f;   // AI assessment of musicality (0.0-1.0)
};

struct RhythmicElement {
    std::string name;               // "Main Beat", "Percussion", etc.
    std::string instrument;         // "Drums", "Percussion", etc.
    std::vector<int> pattern;       // Rhythm pattern (durations in 16th notes)
    int velocity = 80;              // Default velocity
    
    // Rhythmic characteristics
    float swing = 0.0f;            // Amount of swing (0.0-1.0)
    float humanization = 0.1f;      // Timing variation (0.0-1.0)
    std::string aiEnhancements;     // AI rhythm suggestions
};

struct HarmonicStructure {
    std::string key;                    // "C_major", "A_minor", etc.
    std::string timeSignature = "4/4";  // Time signature
    
    // Chord progressions for different sections
    std::string verseProgression;       // "C-Am-F-G"
    std::string chorusProgression;      // "F-G-C-Am"
    std::string bridgeProgression;      // Optional bridge chords
    std::string introProgression;       // Optional intro chords
    std::string outroProgression;       // Optional outro chords
    
    // AI harmonic analysis
    std::string aiSuggestions;          // AI harmonic recommendations
    float harmonyComplexity = 0.5f;     // Complexity assessment (0.0-1.0)
};

struct ArrangementSection {
    std::string name;           // "Intro", "Verse", "Chorus", etc.
    int startBar = 0;          // Starting measure
    int endBar = 0;            // Ending measure
    std::string description;    // Purpose/character of section
    
    // Section characteristics
    float energy = 0.5f;       // Energy level (0.0-1.0)
    float density = 0.5f;      // Instrumental density (0.0-1.0)
    std::vector<std::string> activeInstruments; // Instruments playing in this section
};

struct ArrangementStructure {
    std::vector<ArrangementSection> sections;
    int totalLength = 180;              // Total duration in seconds
    int tempo = 120;                    // BPM
    std::string overallForm;            // "ABABCB", "verse-chorus-verse", etc.
    
    // AI arrangement insights
    std::string aiSuggestions;          // AI arrangement recommendations
    float structuralBalance = 0.5f;     // Balance assessment (0.0-1.0)
};

// ============================================================================
// Generation Request and Result
// ============================================================================

struct GenerationRequest {
    // Basic parameters
    std::string title = "Untitled";
    std::string genre = "Pop";         // Musical genre
    std::string key = "C_major";       // Key signature
    int tempo = 120;                   // BPM
    std::string timeSignature = "4/4"; // Time signature
    int duration = 180;                // Duration in seconds
    
    // Creative parameters
    float creativity = 0.7f;           // How creative/experimental (0.0-1.0)
    float complexity = 0.5f;           // Musical complexity (0.0-1.0)
    float energy = 0.5f;               // Overall energy level (0.0-1.0)
    
    // Style and mood
    std::string mood = "Happy";        // "Happy", "Sad", "Energetic", "Chill", etc.
    std::string targetStyle;           // Target style for style transfer
    std::vector<std::string> influences; // Musical influences or references
    
    // Structure preferences
    std::string structure = "verse-chorus"; // Preferred song structure
    bool includeIntro = true;
    bool includeOutro = true;
    bool includeBridge = false;
    
    // Instrumentation
    std::vector<std::string> instruments; // Desired instruments
    bool allowAIInstrumentSelection = true;
    
    // AI preferences
    bool useAI = true;                 // Use AI for enhancement
    std::string additionalPrompt;      // Custom AI instructions
    
    // Advanced options
    bool enforceKey = true;            // Stay strictly in key
    bool allowDissonance = false;      // Allow dissonant harmonies
    int maxVoices = 4;                // Maximum melodic lines
    float humanization = 0.1f;         // Timing/velocity variation
};

struct CompositionResult {
    bool success = false;
    std::string errorMessage;
    
    // Original request
    GenerationRequest request;
    
    // Generated content
    HarmonicStructure harmonicStructure;
    std::vector<MelodyLine> melodyLines;
    std::vector<RhythmicElement> rhythmicElements;
    ArrangementStructure arrangement;
    
    // Style characteristics (if style transfer was applied)
    StyleTemplate styleCharacteristics;
    
    // AI analysis and feedback
    std::string aiAnalysis;             // Detailed AI analysis of the composition
    std::vector<std::string> aiSuggestions; // AI improvement suggestions
    float confidence = 0.0f;            // AI confidence in the result (0.0-1.0)
    
    // Quality metrics
    float musicalityScore = 0.5f;       // Overall musicality assessment
    float originalityScore = 0.5f;      // How original/unique the composition is
    float coherenceScore = 0.5f;        // How well the parts work together
    
    // Export information
    std::string exportPath;             // Path to exported MIDI file (if exported)
    std::vector<std::string> generatedFiles; // List of generated files
};

// ============================================================================
// AI Composition Engine - Main Music Generator
// ============================================================================

class AICompositionEngine {
public:
    AICompositionEngine();
    ~AICompositionEngine();
    
    // Non-copyable
    AICompositionEngine(const AICompositionEngine&) = delete;
    AICompositionEngine& operator=(const AICompositionEngine&) = delete;
    
    // Initialize composition engine
    bool initialize();
    
    // Main composition generation
    core::AsyncResult<CompositionResult> generateComposition(const GenerationRequest& request);
    
    // Partial generation functions
    core::AsyncResult<HarmonicStructure> generateHarmony(const GenerationRequest& request);
    core::AsyncResult<std::vector<MelodyLine>> generateMelodies(const GenerationRequest& request);
    core::AsyncResult<ArrangementStructure> generateArrangement(const GenerationRequest& request);
    
    // Creativity controls
    void setCreativityLevel(float level);     // 0.0 = conservative, 1.0 = experimental
    void setComplexityLevel(float level);     // 0.0 = simple, 1.0 = complex
    void setOriginalityLevel(float level);    // 0.0 = familiar, 1.0 = unique
    
    // Generation status
    bool isGenerating() const;
    void cancelGeneration();
    
    // Music theory utilities
    std::vector<std::string> getChordProgression(const std::string& key, const std::string& style);
    std::vector<std::string> getScaleNotes(const std::string& key);
    std::vector<int> getRhythmPattern(const std::string& style, const std::string& timeSignature);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// Music Export and Import
// ============================================================================

class MusicExporter {
public:
    // Export to MIDI file
    static bool exportToMIDI(const CompositionResult& composition, const std::string& filePath);
    
    // Export to audio (requires audio rendering)
    static bool exportToAudio(const CompositionResult& composition, const std::string& filePath, const std::string& format = "wav");
    
    // Export to music notation (simplified)
    static bool exportToNotation(const CompositionResult& composition, const std::string& filePath);
    
    // Export composition data to JSON
    static bool exportToJSON(const CompositionResult& composition, const std::string& filePath);
};

class MusicImporter {
public:
    // Import from MIDI file
    static core::Result<CompositionResult> importFromMIDI(const std::string& filePath);
    
    // Import composition from JSON
    static core::Result<CompositionResult> importFromJSON(const std::string& filePath);
    
    // Analyze existing audio for style transfer
    static core::Result<StyleTemplate> analyzeAudioFile(const std::string& filePath);
};

// ============================================================================
// Music Theory and Analysis Utilities
// ============================================================================

namespace theory {
    // Chord analysis and generation
    std::vector<std::string> analyzeChordProgression(const std::string& progression);
    std::string generateChordProgression(const std::string& key, const std::string& style, int length = 4);
    bool isProgressionValid(const std::string& progression, const std::string& key);
    
    // Scale and key utilities
    std::vector<std::string> getNotesInKey(const std::string& key);
    std::string getRelativeMinor(const std::string& majorKey);
    std::string getRelativeMajor(const std::string& minorKey);
    std::vector<std::string> getCompatibleKeys(const std::string& key);
    
    // Rhythm analysis
    float calculateRhythmicComplexity(const std::vector<int>& pattern);
    std::vector<int> generateRhythmVariation(const std::vector<int>& basePattern, float variationAmount = 0.3f);
    
    // Melody analysis
    float analyzeMelodic Contour(const std::vector<MelodyNote>& melody);
    float calculateMelodicInterval(const MelodyNote& note1, const MelodyNote& note2);
    bool isIntervalConsonant(float interval);
    
    // Harmonic analysis
    float calculateHarmonicTension(const std::string& chord, const std::string& key);
    std::vector<std::string> getSuggestedNextChords(const std::string& currentChord, const std::string& key);
    
    // Style analysis
    StyleTemplate analyzeCompositionStyle(const CompositionResult& composition);
    float calculateStyleSimilarity(const CompositionResult& comp1, const CompositionResult& comp2);
}

// ============================================================================
// AI Music Generation Presets
// ============================================================================

namespace presets {
    // Genre presets
    extern const GenerationRequest POP_BALLAD;
    extern const GenerationRequest ROCK_ANTHEM;
    extern const GenerationRequest JAZZ_STANDARD;
    extern const GenerationRequest ELECTRONIC_DANCE;
    extern const GenerationRequest CLASSICAL_SONATA;
    extern const GenerationRequest HIP_HOP_BEAT;
    extern const GenerationRequest AMBIENT_SOUNDSCAPE;
    extern const GenerationRequest FOLK_ACOUSTIC;
    extern const GenerationRequest BLUES_12_BAR;
    extern const GenerationRequest REGGAE_GROOVE;
    
    // Mood presets
    extern const GenerationRequest HAPPY_UPLIFTING;
    extern const GenerationRequest SAD_MELANCHOLIC;
    extern const GenerationRequest ENERGETIC_DRIVING;
    extern const GenerationRequest CALM_PEACEFUL;
    extern const GenerationRequest DRAMATIC_CINEMATIC;
    extern const GenerationRequest MYSTERIOUS_DARK;
    
    // Get all available presets
    std::vector<GenerationRequest> getAllPresets();
    GenerationRequest getPresetByName(const std::string& name);
}

// ============================================================================
// Global Composition Engine Access
// ============================================================================

// Get global composition engine (singleton)
AICompositionEngine& getGlobalCompositionEngine();

// Shutdown composition engine (call at app exit)
void shutdownGlobalCompositionEngine();

} // namespace mixmind::ai