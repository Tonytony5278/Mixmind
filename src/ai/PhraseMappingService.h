#pragma once

#include "Actions.h"
#include <string>
#include <vector>
#include <regex>
#include <functional>
#include <unordered_map>

namespace mixmind::ai {

// Phrase mapping result
struct ParseResult {
    bool success;
    Action action;
    std::string errorMessage;
    std::string helpText;
    double confidence; // 0.0 - 1.0, how confident we are in the mapping
    
    static ParseResult success(const Action& a, double conf = 1.0) {
        return {true, a, "", "", conf};
    }
    
    static ParseResult error(const std::string& error, const std::string& help = "") {
        return {false, {}, error, help, 0.0};
    }
    
    static ParseResult help(const std::string& helpText) {
        return {false, {}, "Unknown command", helpText, 0.0};
    }
};

// Pattern matching rule
struct ChatPattern {
    std::regex pattern;
    std::function<ParseResult(const std::smatch&)> handler;
    std::string description;
    std::string examples;
    double priority; // Higher priority patterns checked first
    
    ChatPattern(const std::string& regex, 
                std::function<ParseResult(const std::smatch&)> h,
                const std::string& desc,
                const std::string& ex,
                double prio = 1.0)
        : pattern(regex, std::regex_constants::icase)
        , handler(h)
        , description(desc)  
        , examples(ex)
        , priority(prio) {}
};

// Simple regex-based phrase mapping service (no network)
class PhraseMappingService {
public:
    PhraseMappingService();
    ~PhraseMappingService();
    
    // Main parsing function
    ParseResult parsePhrase(const std::string& input);
    
    // Help system
    std::string getHelp() const;
    std::string getExamples() const;
    std::vector<std::string> getCommandCategories() const;
    std::string getCategoryHelp(const std::string& category) const;
    
    // Pattern management
    void addPattern(const ChatPattern& pattern);
    void clearPatterns();
    size_t getPatternCount() const;
    
    // Statistics
    struct ParseStats {
        size_t totalParses = 0;
        size_t successfulParses = 0;
        size_t unknownCommands = 0;
        size_t errors = 0;
        std::unordered_map<std::string, size_t> actionCounts;
    };
    
    const ParseStats& getStats() const { return stats_; }
    void resetStats();
    
private:
    std::vector<ChatPattern> patterns_;
    mutable ParseStats stats_;
    
    // Built-in pattern registration
    void registerBuiltInPatterns();
    
    // Pattern handlers for different command categories
    ParseResult handleTempoCommand(const std::smatch& match);
    ParseResult handleTransportCommand(const std::smatch& match);
    ParseResult handleLoopCommand(const std::smatch& match);
    ParseResult handleTrackCommand(const std::smatch& match);
    ParseResult handleGainCommand(const std::smatch& match);
    ParseResult handlePositionCommand(const std::smatch& match);
    ParseResult handleRecordCommand(const std::smatch& match);
    ParseResult handleFadeCommand(const std::smatch& match);
    ParseResult handleNormalizeCommand(const std::smatch& match);
    
    // Helper functions
    std::string preprocessInput(const std::string& input) const;
    double extractNumber(const std::string& text) const;
    std::string extractText(const std::string& text) const;
    bool isValidTrackName(const std::string& name) const;
    
    // Help text generation
    std::string generateFullHelp() const;
    std::string formatExamples(const std::vector<std::string>& examples) const;
};

} // namespace mixmind::ai