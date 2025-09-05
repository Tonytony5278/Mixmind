#include "PhraseMappingService.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace mixmind::ai {

PhraseMappingService::PhraseMappingService() {
    registerBuiltInPatterns();
}

PhraseMappingService::~PhraseMappingService() = default;

ParseResult PhraseMappingService::parsePhrase(const std::string& input) {
    stats_.totalParses++;
    
    std::string cleanInput = preprocessInput(input);
    if (cleanInput.empty()) {
        stats_.errors++;
        return ParseResult::error("Empty input", getHelp());
    }
    
    // Sort patterns by priority (highest first)
    std::vector<ChatPattern*> sortedPatterns;
    for (auto& pattern : patterns_) {
        sortedPatterns.push_back(&pattern);
    }
    std::sort(sortedPatterns.begin(), sortedPatterns.end(), 
              [](const ChatPattern* a, const ChatPattern* b) {
                  return a->priority > b->priority;
              });
    
    // Try each pattern
    for (const auto* pattern : sortedPatterns) {
        std::smatch match;
        if (std::regex_search(cleanInput, match, pattern->pattern)) {
            auto result = pattern->handler(match);
            if (result.success) {
                stats_.successfulParses++;
                
                // Update action counts
                std::string actionType = getActionTypeName(result.action);
                stats_.actionCounts[actionType]++;
                
                return result;
            }
        }
    }
    
    // No match found
    stats_.unknownCommands++;
    return ParseResult::help(getHelp());
}

std::string PhraseMappingService::getHelp() const {
    return R"(MixMind AI Voice Commands:

TRANSPORT:
- "play" / "start" / "go"
- "stop" / "halt"  
- "play from start" / "restart"
- "record" / "rec on" / "start recording"

TEMPO:
- "set tempo to 120" / "120 bpm" / "tempo 140"
- "faster" / "slower" (±10 BPM)
- "double time" / "half time"

TRACKS:
- "add audio track" / "new audio track called Guitar"
- "add midi track Piano" / "create midi track Drums"
- "mute track 1" / "solo track 2"

GAIN/VOLUME:
- "set track 1 gain to -6" / "track 2 volume -3db"
- "increase gain track 1" / "decrease volume track 2"
- "normalize track 1" / "normalize track 2 to -14 lufs"

POSITION:
- "go to start" / "jump to beginning" / "rewind"
- "go to 2:30" / "jump to bar 16" / "seek to 45 seconds"

LOOP:
- "loop from 0 to 8" / "set loop 4 bars"
- "loop on" / "loop off" / "toggle loop"

FADES:
- "fade in clip 1" / "fade out clip 2"
- "fade in 2 seconds clip 5"

Type 'help [category]' for specific examples, or just describe what you want to do!)";
}

std::string PhraseMappingService::getExamples() const {
    return R"(Example Voice Commands:

"set tempo to 128" → SetTempo{bpm=128}
"play from start" → PlayTransport{fromStart=true}
"add audio track Guitar" → AddAudioTrack{name="Guitar"}
"increase gain track 1 by 3db" → AdjustGain{track=1, dB=3}
"loop from 0 to 16 beats" → SetLoop{start=0, end=16}
"go to 2:30" → SetCursor{pos=150} (in seconds)
"normalize track 2" → Normalize{track=2, target=-23 LUFS}
"fade in clip 5 for 1 second" → FadeIn{clip=5, duration=1000ms}
"stop and return to start" → StopTransport{returnToStart=true}
"record on" → ToggleRecording{enable=true})";
}

void PhraseMappingService::registerBuiltInPatterns() {
    // Clear existing patterns
    patterns_.clear();
    
    // TRANSPORT PATTERNS (High Priority)
    addPattern(ChatPattern(
        R"(^(?:play|start|go)(?:\s+from\s+(?:start|beginning))?$)",
        [this](const std::smatch& m) { return handleTransportCommand(m); },
        "Transport control",
        "play, play from start, go",
        9.0
    ));
    
    addPattern(ChatPattern(
        R"(^(?:stop|halt)(?:\s+(?:and\s+)?(?:return\s+to\s+start|rewind))?$)",
        [this](const std::smatch& m) { return handleTransportCommand(m); },
        "Stop transport",
        "stop, stop and return to start",
        9.0
    ));
    
    addPattern(ChatPattern(
        R"(^(?:record|rec\s+on|start\s+recording)$)",
        [this](const std::smatch& m) { return handleRecordCommand(m); },
        "Recording control",
        "record, rec on, start recording",
        9.0
    ));
    
    // TEMPO PATTERNS (High Priority)
    addPattern(ChatPattern(
        R"(^(?:set\s+)?tempo\s+(?:to\s+)?(\d+(?:\.\d+)?)(?:\s+bpm)?$)",
        [this](const std::smatch& m) { return handleTempoCommand(m); },
        "Set tempo",
        "tempo 120, set tempo to 140, tempo 128 bpm",
        8.5
    ));
    
    addPattern(ChatPattern(
        R"(^(\d+(?:\.\d+)?)\s*bpm$)",
        [this](const std::smatch& m) { return handleTempoCommand(m); },
        "BPM shorthand", 
        "120 bpm, 140bpm",
        8.0
    ));
    
    addPattern(ChatPattern(
        R"(^(?:faster|tempo\s+up)(?:\s+(\d+))?$)",
        [this](const std::smatch& m) { return handleTempoCommand(m); },
        "Increase tempo",
        "faster, tempo up, faster 5",
        7.5
    ));
    
    addPattern(ChatPattern(
        R"(^(?:slower|tempo\s+down)(?:\s+(\d+))?$)",
        [this](const std::smatch& m) { return handleTempoCommand(m); },
        "Decrease tempo",
        "slower, tempo down, slower 10",
        7.5
    ));
    
    // TRACK PATTERNS
    addPattern(ChatPattern(
        R"(^(?:add|create|new)\s+audio\s+track(?:\s+(?:called|named)\s+)?(.*)$)",
        [this](const std::smatch& m) { return handleTrackCommand(m); },
        "Add audio track",
        "add audio track, new audio track Guitar, create audio track called Bass",
        7.0
    ));
    
    addPattern(ChatPattern(
        R"(^(?:add|create|new)\s+midi\s+track(?:\s+(?:called|named)\s+)?(.*)$)",
        [this](const std::smatch& m) { return handleTrackCommand(m); },
        "Add MIDI track", 
        "add midi track, new midi track Piano, create midi track Drums",
        7.0
    ));
    
    // GAIN/VOLUME PATTERNS
    addPattern(ChatPattern(
        R"(^(?:set\s+)?track\s+(\d+)\s+(?:gain|volume)\s+(?:to\s+)?(-?\d+(?:\.\d+)?)(?:\s*db)?$)",
        [this](const std::smatch& m) { return handleGainCommand(m); },
        "Set track gain",
        "track 1 gain -6, set track 2 volume to -3db",
        6.5
    ));
    
    addPattern(ChatPattern(
        R"(^(?:increase|raise|boost)\s+(?:gain|volume)\s+track\s+(\d+)(?:\s+(?:by\s+)?(\d+(?:\.\d+)?)(?:\s*db)?)?$)",
        [this](const std::smatch& m) { return handleGainCommand(m); },
        "Increase gain",
        "increase gain track 1, boost volume track 2 by 3db",
        6.0
    ));
    
    addPattern(ChatPattern(
        R"(^(?:decrease|lower|reduce)\s+(?:gain|volume)\s+track\s+(\d+)(?:\s+(?:by\s+)?(\d+(?:\.\d+)?)(?:\s*db)?)?$)",
        [this](const std::smatch& m) { return handleGainCommand(m); },
        "Decrease gain",
        "decrease gain track 1, reduce volume track 2 by 6db",
        6.0
    ));
    
    // NORMALIZE PATTERNS
    addPattern(ChatPattern(
        R"(^normalize\s+track\s+(\d+)(?:\s+(?:to\s+)?(-?\d+(?:\.\d+)?)\s*(?:lufs?|db)?)?$)",
        [this](const std::smatch& m) { return handleNormalizeCommand(m); },
        "Normalize track",
        "normalize track 1, normalize track 2 to -14 lufs",
        6.0
    ));
    
    // POSITION PATTERNS
    addPattern(ChatPattern(
        R"(^(?:go\s+to|jump\s+to|seek\s+to)\s+(?:start|beginning|0)$)",
        [this](const std::smatch& m) { return handlePositionCommand(m); },
        "Go to start",
        "go to start, jump to beginning",
        5.5
    ));
    
    addPattern(ChatPattern(
        R"(^(?:go\s+to|jump\s+to|seek\s+to)\s+(\d+):(\d+)(?::(\d+))?$)",
        [this](const std::smatch& m) { return handlePositionCommand(m); },
        "Go to time position",
        "go to 2:30, jump to 1:45:50",
        5.5
    ));
    
    addPattern(ChatPattern(
        R"(^(?:go\s+to|jump\s+to|seek\s+to)\s+(?:bar\s+)?(\d+)(?:\s+(?:bars?|beats?))?$)",
        [this](const std::smatch& m) { return handlePositionCommand(m); },
        "Go to bar/beat",
        "go to bar 16, jump to 32 beats",
        5.0
    ));
    
    // LOOP PATTERNS
    addPattern(ChatPattern(
        R"(^loop\s+from\s+(\d+(?:\.\d+)?)\s+to\s+(\d+(?:\.\d+)?)(?:\s+(?:bars?|beats?))?$)",
        [this](const std::smatch& m) { return handleLoopCommand(m); },
        "Set loop range",
        "loop from 0 to 8, loop from 4 to 20 beats",
        5.5
    ));
    
    addPattern(ChatPattern(
        R"(^(?:set\s+)?loop\s+(\d+)\s+(?:bars?|beats?)$)",
        [this](const std::smatch& m) { return handleLoopCommand(m); },
        "Set loop length",
        "loop 8 bars, set loop 16 beats",
        5.0
    ));
    
    addPattern(ChatPattern(
        R"(^loop\s+(?:on|off|toggle)$)",
        [this](const std::smatch& m) { return handleLoopCommand(m); },
        "Toggle loop",
        "loop on, loop off, loop toggle",
        5.0
    ));
    
    // FADE PATTERNS
    addPattern(ChatPattern(
        R"(^fade\s+(in|out)\s+clip\s+(\d+)(?:\s+(?:for\s+)?(\d+)\s*(?:ms|seconds?|s))?$)",
        [this](const std::smatch& m) { return handleFadeCommand(m); },
        "Fade clip",
        "fade in clip 1, fade out clip 2 for 2 seconds",
        4.5
    ));
    
    // HELP PATTERNS (Low Priority)
    addPattern(ChatPattern(
        R"(^help(?:\s+(.+))?$)",
        [this](const std::smatch& m) -> ParseResult {
            std::string category = m.size() > 1 ? m[1].str() : "";
            if (category.empty()) {
                return ParseResult::help(getHelp());
            } else {
                return ParseResult::help(getCategoryHelp(category));
            }
        },
        "Help system",
        "help, help tempo, help tracks",
        1.0
    ));
}

// Pattern Handler Implementations

ParseResult PhraseMappingService::handleTempoCommand(const std::smatch& match) {
    std::string fullMatch = match[0].str();
    
    if (std::regex_search(fullMatch, std::regex("faster|tempo\\s+up"))) {
        // Default +10 BPM, or custom amount
        double increase = 10.0;
        if (match.size() > 1 && !match[1].str().empty()) {
            increase = extractNumber(match[1].str());
        }
        // We'd need current tempo from app state - for now use reasonable default
        return ParseResult::success(SetTempo{120.0 + increase}, 0.8);
    }
    
    if (std::regex_search(fullMatch, std::regex("slower|tempo\\s+down"))) {
        double decrease = 10.0;
        if (match.size() > 1 && !match[1].str().empty()) {
            decrease = extractNumber(match[1].str());
        }
        return ParseResult::success(SetTempo{120.0 - decrease}, 0.8);
    }
    
    // Direct tempo setting
    if (match.size() > 1) {
        double bpm = extractNumber(match[1].str());
        if (bpm > 0.0 && bpm <= 300.0) {
            return ParseResult::success(SetTempo{bpm});
        }
    }
    
    return ParseResult::error("Invalid tempo value", "Try: 'tempo 120' or '140 bpm'");
}

ParseResult PhraseMappingService::handleTransportCommand(const std::smatch& match) {
    std::string fullMatch = match[0].str();
    
    if (std::regex_search(fullMatch, std::regex("play|start|go"))) {
        bool fromStart = std::regex_search(fullMatch, std::regex("from\\s+start|from\\s+beginning"));
        return ParseResult::success(PlayTransport{fromStart});
    }
    
    if (std::regex_search(fullMatch, std::regex("stop|halt"))) {
        bool returnToStart = std::regex_search(fullMatch, std::regex("return\\s+to\\s+start|rewind"));
        return ParseResult::success(StopTransport{returnToStart});
    }
    
    return ParseResult::error("Unknown transport command");
}

ParseResult PhraseMappingService::handleRecordCommand(const std::smatch& match) {
    return ParseResult::success(ToggleRecording{true});
}

ParseResult PhraseMappingService::handleTrackCommand(const std::smatch& match) {
    std::string fullMatch = match[0].str();
    std::string trackName = "New Track";
    
    if (match.size() > 1 && !match[1].str().empty()) {
        trackName = extractText(match[1].str());
    }
    
    if (!isValidTrackName(trackName)) {
        return ParseResult::error("Invalid track name", "Track names must be 1-64 characters");
    }
    
    if (std::regex_search(fullMatch, std::regex("audio"))) {
        return ParseResult::success(AddAudioTrack{trackName});
    } else if (std::regex_search(fullMatch, std::regex("midi"))) {
        return ParseResult::success(AddMidiTrack{trackName});
    }
    
    return ParseResult::error("Unknown track type");
}

ParseResult PhraseMappingService::handleGainCommand(const std::smatch& match) {
    std::string fullMatch = match[0].str();
    
    if (match.size() < 2) {
        return ParseResult::error("Track index required", "Try: 'track 1 gain -6'");
    }
    
    int trackIndex = static_cast<int>(extractNumber(match[1].str()));
    double gainChange = 0.0;
    
    if (std::regex_search(fullMatch, std::regex("set.*gain|set.*volume"))) {
        // Direct gain setting: "set track 1 gain to -6"
        if (match.size() > 2) {
            gainChange = extractNumber(match[2].str());
        }
    } else if (std::regex_search(fullMatch, std::regex("increase|raise|boost"))) {
        // Relative gain increase
        gainChange = 3.0; // Default +3dB
        if (match.size() > 2 && !match[2].str().empty()) {
            gainChange = extractNumber(match[2].str());
        }
        // We'd need current gain from app state - this is a relative operation
    } else if (std::regex_search(fullMatch, std::regex("decrease|lower|reduce"))) {
        // Relative gain decrease  
        gainChange = -3.0; // Default -3dB
        if (match.size() > 2 && !match[2].str().empty()) {
            gainChange = -extractNumber(match[2].str());
        }
    }
    
    return ParseResult::success(AdjustGain{trackIndex, gainChange});
}

ParseResult PhraseMappingService::handleNormalizeCommand(const std::smatch& match) {
    if (match.size() < 2) {
        return ParseResult::error("Track index required", "Try: 'normalize track 1'");
    }
    
    int trackIndex = static_cast<int>(extractNumber(match[1].str()));
    double targetLUFS = -23.0; // EBU R128 standard
    
    if (match.size() > 2 && !match[2].str().empty()) {
        targetLUFS = extractNumber(match[2].str());
        if (targetLUFS > -6.0) targetLUFS = -targetLUFS; // Handle positive input
    }
    
    return ParseResult::success(Normalize{trackIndex, targetLUFS});
}

ParseResult PhraseMappingService::handlePositionCommand(const std::smatch& match) {
    std::string fullMatch = match[0].str();
    
    if (std::regex_search(fullMatch, std::regex("start|beginning"))) {
        return ParseResult::success(SetCursor{0.0});
    }
    
    // Time format: MM:SS or MM:SS:MS
    if (match.size() >= 3) {
        double minutes = extractNumber(match[1].str());
        double seconds = extractNumber(match[2].str());
        double ms = 0.0;
        if (match.size() > 3 && !match[3].str().empty()) {
            ms = extractNumber(match[3].str()) / 1000.0;
        }
        
        double totalSeconds = minutes * 60.0 + seconds + ms;
        return ParseResult::success(SetCursor{totalSeconds});
    }
    
    // Bar/beat format
    if (match.size() >= 2) {
        double position = extractNumber(match[1].str());
        // Convert bars to beats (assuming 4/4 time)
        if (std::regex_search(fullMatch, std::regex("bar"))) {
            position *= 4.0; // 4 beats per bar
        }
        return ParseResult::success(SetCursor{position});
    }
    
    return ParseResult::error("Invalid position format", "Try: 'go to 2:30' or 'jump to bar 16'");
}

ParseResult PhraseMappingService::handleLoopCommand(const std::smatch& match) {
    std::string fullMatch = match[0].str();
    
    if (std::regex_search(fullMatch, std::regex("loop\\s+on"))) {
        // Enable loop with default range
        return ParseResult::success(SetLoop{0.0, 8.0});
    }
    
    if (std::regex_search(fullMatch, std::regex("from.*to"))) {
        // "loop from X to Y"
        if (match.size() >= 3) {
            double start = extractNumber(match[1].str());
            double end = extractNumber(match[2].str());
            return ParseResult::success(SetLoop{start, end});
        }
    }
    
    if (std::regex_search(fullMatch, std::regex("loop\\s+\\d+"))) {
        // "loop 8 bars" 
        if (match.size() >= 2) {
            double length = extractNumber(match[1].str());
            return ParseResult::success(SetLoop{0.0, length});
        }
    }
    
    return ParseResult::error("Invalid loop command", "Try: 'loop from 0 to 8' or 'loop 4 bars'");
}

ParseResult PhraseMappingService::handleFadeCommand(const std::smatch& match) {
    if (match.size() < 3) {
        return ParseResult::error("Clip ID required", "Try: 'fade in clip 1'");
    }
    
    std::string fadeType = match[1].str();
    int clipId = static_cast<int>(extractNumber(match[2].str()));
    int durationMs = 1000; // Default 1 second
    
    if (match.size() > 3 && !match[3].str().empty()) {
        double duration = extractNumber(match[3].str());
        std::string fullMatch = match[0].str();
        
        // Check if it's in seconds or milliseconds
        if (std::regex_search(fullMatch, std::regex("seconds?|s"))) {
            durationMs = static_cast<int>(duration * 1000.0);
        } else {
            durationMs = static_cast<int>(duration);
        }
    }
    
    if (fadeType == "in") {
        return ParseResult::success(FadeIn{clipId, durationMs});
    } else if (fadeType == "out") {
        return ParseResult::success(FadeOut{clipId, durationMs});
    }
    
    return ParseResult::error("Invalid fade type", "Use 'fade in' or 'fade out'");
}

// Helper Functions

std::string PhraseMappingService::preprocessInput(const std::string& input) const {
    std::string result = input;
    
    // Convert to lowercase
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    
    // Trim whitespace
    result.erase(0, result.find_first_not_of(" \t\r\n"));
    result.erase(result.find_last_not_of(" \t\r\n") + 1);
    
    // Normalize common abbreviations
    result = std::regex_replace(result, std::regex("\\bdb\\b"), "db");
    result = std::regex_replace(result, std::regex("\\bbpm\\b"), "bpm");
    result = std::regex_replace(result, std::regex("\\bms\\b"), "ms");
    
    return result;
}

double PhraseMappingService::extractNumber(const std::string& text) const {
    std::string cleanText = text;
    // Remove non-numeric characters except decimal point and minus
    cleanText = std::regex_replace(cleanText, std::regex("[^0-9.-]"), "");
    
    try {
        return std::stod(cleanText);
    } catch (...) {
        return 0.0;
    }
}

std::string PhraseMappingService::extractText(const std::string& text) const {
    std::string result = text;
    
    // Trim whitespace
    result.erase(0, result.find_first_not_of(" \t\r\n"));
    result.erase(result.find_last_not_of(" \t\r\n") + 1);
    
    // Remove quotes if present
    if (!result.empty() && result[0] == '"' && result.back() == '"') {
        result = result.substr(1, result.length() - 2);
    }
    
    return result;
}

bool PhraseMappingService::isValidTrackName(const std::string& name) const {
    return !name.empty() && name.length() <= 64;
}

std::vector<std::string> PhraseMappingService::getCommandCategories() const {
    return {"transport", "tempo", "tracks", "gain", "position", "loop", "fades", "normalize"};
}

std::string PhraseMappingService::getCategoryHelp(const std::string& category) const {
    if (category == "transport") {
        return R"(TRANSPORT COMMANDS:
- play, start, go
- play from start, restart  
- stop, halt
- stop and return to start
- record, rec on, start recording)";
    }
    
    if (category == "tempo") {
        return R"(TEMPO COMMANDS:
- tempo 120, set tempo to 140
- 128 bpm, 140bpm
- faster, tempo up, faster 5
- slower, tempo down, slower 10)";
    }
    
    if (category == "tracks") {
        return R"(TRACK COMMANDS:
- add audio track, new audio track Guitar
- add midi track Piano, create midi track Drums
- mute track 1, solo track 2)";
    }
    
    return "Unknown category. Available: " + [this]() {
        auto cats = getCommandCategories();
        std::ostringstream oss;
        for (size_t i = 0; i < cats.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << cats[i];
        }
        return oss.str();
    }();
}

void PhraseMappingService::addPattern(const ChatPattern& pattern) {
    patterns_.push_back(pattern);
}

void PhraseMappingService::clearPatterns() {
    patterns_.clear();
}

size_t PhraseMappingService::getPatternCount() const {
    return patterns_.size();
}

void PhraseMappingService::resetStats() {
    stats_ = ParseStats{};
}

} // namespace mixmind::ai