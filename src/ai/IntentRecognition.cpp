#include "IntentRecognition.h"
#include "../core/async.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <unordered_set>
#include <shared_mutex>

namespace mixmind::ai {

// ============================================================================
// Built-in Intent Patterns for Audio Production
// ============================================================================

namespace patterns {

const std::vector<std::string> TRANSPORT_PATTERNS = {
    R"(\b(play|start|begin)\b)",
    R"(\b(stop|halt|pause)\b)",
    R"(\b(record|rec|recording)\b)",
    R"(\b(tempo|bpm)\s*(\d+))",
    R"(\bset\s+tempo\s+to\s+(\d+))",
    R"(\bchange\s+tempo)",
    R"(\brewind\b)",
    R"(\bfast\s*forward\b)"
};

const std::vector<std::string> TRACK_PATTERNS = {
    R"(\b(create|new|add)\s+(track|channel))",
    R"(\b(delete|remove)\s+track\s*(\d+)?)",
    R"(\b(mute|unmute)\s*(track\s*(\d+)?)?)",
    R"(\b(solo|unsolo)\s*(track\s*(\d+)?)?)",
    R"(\bvolume\s*(track\s*(\d+)?)?\s*(to\s*)?(\d+|up|down))",
    R"(\bpan\s*(track\s*(\d+)?)?\s*(left|right|center|\d+))",
    R"(\b(arm|unarm)\s*(track\s*(\d+)?)?)"
};

const std::vector<std::string> MIXING_PATTERNS = {
    R"(\b(eq|equalization)\b)",
    R"(\b(compressor|compression)\b)",
    R"(\b(reverb|delay|echo)\b)",
    R"(\b(gain|volume)\s*(up|down|\+\d+|\-\d+))",
    R"(\b(add|insert)\s+(plugin|effect))",
    R"(\b(remove|delete)\s+(plugin|effect))",
    R"(\bautomate\s+(\w+))"
};

const std::vector<std::string> RECORDING_PATTERNS = {
    R"(\brecord\s+(audio|midi))",
    R"(\bpunch\s+(in|out))",
    R"(\boverdub\b)",
    R"(\bmetronome\s+(on|off))",
    R"(\bcount\s*in\b)",
    R"(\binput\s+(monitoring|gain))"
};

const std::vector<std::string> EDITING_PATTERNS = {
    R"(\b(cut|copy|paste)\b)",
    R"(\b(split|slice)\s*(at\s*bar\s*(\d+))?)",
    R"(\b(trim|crop)\b)",
    R"(\b(fade\s*(in|out))\b)",
    R"(\b(normalize|quantize)\b)",
    R"(\b(undo|redo)\b)"
};

const std::vector<std::string> NAVIGATION_PATTERNS = {
    R"(\bgo\s+to\s+(bar|measure)\s*(\d+))",
    R"(\bgo\s+to\s+(beginning|start|end))",
    R"(\bzoom\s*(in|out))",
    R"(\bselect\s+(all|none))",
    R"(\bfocus\s+(track\s*(\d+)?)",
    R"(\bloop\s+(on|off|enable|disable))"
};

const std::vector<std::string> QUERY_PATTERNS = {
    R"(\bwhat\s+(is|are))",
    R"(\bhow\s+(do|can)\s+i)",
    R"(\bwhere\s+(is|are))",
    R"(\bshow\s+me)",
    R"(\btell\s+me\s+about)",
    R"(\bwhat's\s+the\s+(current|current\s+)?(tempo|time|position))"
};

const std::vector<std::string> HELP_PATTERNS = {
    R"(\bhelp\b)",
    R"(\bassist(ance)?)",
    R"(\bguide\b)",
    R"(\btutorial\b)",
    R"(\bhow\s+to\b)",
    R"(\bi\s+(don't\s+know|need\s+help))"
};

} // namespace patterns

// ============================================================================
// IntentRecognition Implementation
// ============================================================================

IntentRecognition::IntentRecognition() {
    // Initialize built-in knowledge
}

IntentRecognition::~IntentRecognition() {
    if (isInitialized_.load()) {
        shutdown().get();
    }
}

// ========================================================================
// Service Lifecycle
// ========================================================================

core::AsyncResult<core::VoidResult> IntentRecognition::initialize() {
    return core::getGlobalThreadPool().executeAsyncVoid([this]() -> core::VoidResult {
        std::unique_lock lock(contextMutex_);
        
        if (isInitialized_.load()) {
            return core::VoidResult::success();
        }
        
        // Initialize components
        featureExtractor_ = std::make_unique<FeatureExtractor>();
        patternMatcher_ = std::make_unique<PatternMatcher>();
        entityRecognizer_ = std::make_unique<EntityRecognizer>();
        contextProcessor_ = std::make_unique<ContextProcessor>();
        learningEngine_ = std::make_unique<LearningEngine>();
        
        // Initialize built-in knowledge
        initializeBuiltInKnowledge();
        
        // Load domain knowledge
        loadAudioProductionDomain();
        
        // Initialize entity recognizers
        initializeEntityRecognizers();
        
        // Reset statistics
        {
            std::lock_guard statsLock(statsMutex_);
            stats_ = RecognitionStats{};
        }
        
        isInitialized_.store(true);
        knowledgeLoaded_.store(true);
        
        return core::VoidResult::success();
    }, "Initializing IntentRecognition");
}

core::AsyncResult<core::VoidResult> IntentRecognition::shutdown() {
    return core::getGlobalThreadPool().executeAsyncVoid([this]() -> core::VoidResult {
        std::unique_lock lock(contextMutex_);
        
        if (!isInitialized_.load()) {
            return core::VoidResult::success();
        }
        
        // Clear all data
        conversationContexts_.clear();
        domainConcepts_.clear();
        intentPatterns_.clear();
        entityPatterns_.clear();
        
        // Cleanup components
        featureExtractor_.reset();
        patternMatcher_.reset();
        entityRecognizer_.reset();
        contextProcessor_.reset();
        learningEngine_.reset();
        
        isInitialized_.store(false);
        knowledgeLoaded_.store(false);
        
        return core::VoidResult::success();
    }, "Shutting down IntentRecognition");
}

bool IntentRecognition::isReady() const {
    return isInitialized_.load() && knowledgeLoaded_.load();
}

// ========================================================================
// Intent Classification
// ========================================================================

core::AsyncResult<core::Result<IntentClassification>> IntentRecognition::classifyIntent(
    const std::string& input,
    const IntentRecognitionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<IntentClassification>>(
        [this, input, context]() -> core::Result<IntentClassification> {
            
            if (!isReady()) {
                return core::Result<IntentClassification>::error(
                    core::ErrorCode::NotInitialized,
                    core::ErrorCategory::general(),
                    "Intent recognition not initialized"
                );
            }
            
            // Extract features from input
            IntentFeatures features = extractFeatures(input);
            
            // Classify intent using pattern matching
            IntentClassification classification = classifyUsingPatterns(input, features);
            
            // Enrich with context
            enrichWithContext(classification, context);
            
            // Update statistics
            updateStats(classification);
            
            return core::Result<IntentClassification>::success(classification);
        },
        "Classifying intent"
    );
}

core::AsyncResult<core::Result<std::vector<IntentClassification>>> IntentRecognition::getRankedIntents(
    const std::string& input,
    size_t maxResults,
    const IntentRecognitionContext& context) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<std::vector<IntentClassification>>>(
        [this, input, maxResults, context]() -> core::Result<std::vector<IntentClassification>> {
            
            // Get multiple classifications
            std::vector<IntentClassification> classifications = getAllClassifications(input, context);
            
            // Sort by confidence
            std::sort(classifications.begin(), classifications.end(),
                [](const IntentClassification& a, const IntentClassification& b) {
                    return a.confidence > b.confidence;
                });
            
            // Limit results
            if (classifications.size() > maxResults) {
                classifications.resize(maxResults);
            }
            
            return core::Result<std::vector<IntentClassification>>::success(classifications);
        },
        "Getting ranked intents"
    );
}

// ========================================================================
// Entity Recognition
// ========================================================================

core::AsyncResult<core::Result<std::vector<Entity>>> IntentRecognition::extractEntities(
    const std::string& input,
    const std::vector<EntityType>& targetTypes) {
    
    return core::getGlobalThreadPool().executeAsync<core::Result<std::vector<Entity>>>(
        [this, input, targetTypes]() -> core::Result<std::vector<Entity>> {
            
            std::vector<Entity> entities;
            
            // Extract different entity types
            auto numberEntities = extractNumberEntities(input);
            auto trackEntities = extractTrackEntities(input);
            auto timeEntities = extractTimeEntities(input);
            auto audioEntities = extractAudioTermEntities(input);
            
            // Combine all entities
            entities.insert(entities.end(), numberEntities.begin(), numberEntities.end());
            entities.insert(entities.end(), trackEntities.begin(), trackEntities.end());
            entities.insert(entities.end(), timeEntities.begin(), timeEntities.end());
            entities.insert(entities.end(), audioEntities.begin(), audioEntities.end());
            
            // Filter by target types if specified
            if (!targetTypes.empty()) {
                entities.erase(
                    std::remove_if(entities.begin(), entities.end(),
                        [&targetTypes](const Entity& entity) {
                            return std::find(targetTypes.begin(), targetTypes.end(), entity.type) == targetTypes.end();
                        }),
                    entities.end()
                );
            }
            
            return core::Result<std::vector<Entity>>::success(entities);
        },
        "Extracting entities"
    );
}

// ========================================================================
// Context Management
// ========================================================================

core::VoidResult IntentRecognition::updateConversationContext(
    const std::string& conversationId,
    const std::string& intent,
    const std::unordered_map<std::string, std::string>& stateChanges) {
    
    std::unique_lock lock(contextMutex_);
    
    auto& context = conversationContexts_[conversationId];
    
    // Update recent intents
    context.recentIntents.push_back(intent);
    if (context.recentIntents.size() > 10) { // Keep last 10
        context.recentIntents.erase(context.recentIntents.begin());
    }
    
    // Update session state
    for (const auto& [key, value] : stateChanges) {
        context.sessionState[key] = value;
    }
    
    return core::VoidResult::success();
}

std::optional<ConversationContext> IntentRecognition::getConversationContext(const std::string& conversationId) const {
    std::shared_lock lock(contextMutex_);
    
    auto it = conversationContexts_.find(conversationId);
    if (it != conversationContexts_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// ========================================================================
// Statistics
// ========================================================================

IntentRecognition::RecognitionStats IntentRecognition::getRecognitionStats() const {
    std::lock_guard lock(statsMutex_);
    return stats_;
}

// ========================================================================
// Internal Implementation
// ========================================================================

void IntentRecognition::initializeBuiltInKnowledge() {
    // Initialize intent patterns
    intentPatterns_["transport"] = patterns::TRANSPORT_PATTERNS;
    intentPatterns_["track"] = patterns::TRACK_PATTERNS;
    intentPatterns_["mixing"] = patterns::MIXING_PATTERNS;
    intentPatterns_["recording"] = patterns::RECORDING_PATTERNS;
    intentPatterns_["editing"] = patterns::EDITING_PATTERNS;
    intentPatterns_["navigation"] = patterns::NAVIGATION_PATTERNS;
    intentPatterns_["query"] = patterns::QUERY_PATTERNS;
    intentPatterns_["help"] = patterns::HELP_PATTERNS;
}

void IntentRecognition::loadAudioProductionDomain() {
    // Add domain concepts for audio production
    
    // Transport concepts
    {
        DomainConcept concept;
        concept.concept = "transport";
        concept.domain = AudioProductionDomain::Workflow;
        concept.synonyms = {"playback", "play control", "transport control"};
        concept.relatedConcepts = {"play", "stop", "record", "tempo"};
        concept.definition = "Controls for audio playback and recording";
        concept.typicalActions = {"play", "stop", "record", "pause", "rewind"};
        
        domainConcepts_["transport"] = concept;
    }
    
    // Track concepts
    {
        DomainConcept concept;
        concept.concept = "track";
        concept.domain = AudioProductionDomain::Composition;
        concept.synonyms = {"channel", "strip"};
        concept.relatedConcepts = {"volume", "pan", "mute", "solo", "record"};
        concept.definition = "A single audio or MIDI channel in the DAW";
        concept.typicalActions = {"create", "delete", "mute", "solo", "arm"};
        
        domainConcepts_["track"] = concept;
    }
    
    // Effects concepts
    {
        DomainConcept concept;
        concept.concept = "effect";
        concept.domain = AudioProductionDomain::Mixing;
        concept.synonyms = {"plugin", "processor", "fx"};
        concept.relatedConcepts = {"eq", "compressor", "reverb", "delay"};
        concept.definition = "Audio processing plugin or effect";
        concept.typicalActions = {"add", "remove", "bypass", "automate"};
        
        domainConcepts_["effect"] = concept;
    }
}

void IntentRecognition::initializeEntityRecognizers() {
    // Initialize entity patterns
    
    // Numbers
    entityPatterns_[EntityType::Number] = {
        std::regex(R"(\b\d+\b)"),
        std::regex(R"(\b\d+\.\d+\b)")
    };
    
    // Track references
    entityPatterns_[EntityType::Track] = {
        std::regex(R"(\btrack\s*(\d+)\b)", std::regex::icase),
        std::regex(R"(\bchannel\s*(\d+)\b)", std::regex::icase)
    };
    
    // Time references
    entityPatterns_[EntityType::Time] = {
        std::regex(R"(\bbar\s*(\d+)\b)", std::regex::icase),
        std::regex(R"(\bmeasure\s*(\d+)\b)", std::regex::icase),
        std::regex(R"(\b(\d+):(\d+):(\d+)\b)") // timestamp
    };
    
    // Frequency
    entityPatterns_[EntityType::Frequency] = {
        std::regex(R"(\b(\d+)\s*hz\b)", std::regex::icase),
        std::regex(R"(\b(\d+)\s*khz\b)", std::regex::icase)
    };
    
    // Level (dB, percentage)
    entityPatterns_[EntityType::Level] = {
        std::regex(R"(\b(-?\d+(?:\.\d+)?)\s*db\b)", std::regex::icase),
        std::regex(R"(\b(\d+)\s*%\b)")
    };
}

IntentFeatures IntentRecognition::extractFeatures(const std::string& input) const {
    IntentFeatures features;
    
    // Simple tokenization
    std::istringstream iss(input);
    std::string word;
    
    while (iss >> word) {
        // Convert to lowercase for analysis
        std::string lowerWord = word;
        std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);
        
        // Remove punctuation
        lowerWord.erase(std::remove_if(lowerWord.begin(), lowerWord.end(), 
            [](char c) { return std::ispunct(c); }), lowerWord.end());
        
        if (lowerWord.empty()) continue;
        
        features.keywords.push_back(lowerWord);
        
        // Categorize words
        if (isActionWord(lowerWord)) {
            features.actionWords.push_back(lowerWord);
        }
        
        if (isObjectWord(lowerWord)) {
            features.objectWords.push_back(lowerWord);
        }
        
        if (isAudioTerm(lowerWord)) {
            features.audioTerms.push_back(lowerWord);
        }
    }
    
    // Grammatical features
    features.hasQuestion = input.find('?') != std::string::npos;
    features.hasNegation = input.find("not") != std::string::npos || input.find("n't") != std::string::npos;
    
    return features;
}

IntentClassification IntentRecognition::classifyUsingPatterns(const std::string& input, const IntentFeatures& features) const {
    IntentClassification classification;
    classification.confidence = 0.0;
    classification.type = IntentType::Command; // default
    
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    
    // Check transport patterns
    if (matchesPatterns(lowerInput, patterns::TRANSPORT_PATTERNS)) {
        classification.specificIntent = classifyTransportIntent(lowerInput);
        classification.domain = AudioProductionDomain::Workflow;
        classification.confidence = 0.9;
        
        if (lowerInput.find("play") != std::string::npos) {
            classification.specificIntent = "transport_play";
        } else if (lowerInput.find("stop") != std::string::npos) {
            classification.specificIntent = "transport_stop";
        } else if (lowerInput.find("record") != std::string::npos) {
            classification.specificIntent = "transport_record";
        } else if (lowerInput.find("tempo") != std::string::npos) {
            classification.specificIntent = "transport_set_tempo";
        }
    }
    // Check track patterns
    else if (matchesPatterns(lowerInput, patterns::TRACK_PATTERNS)) {
        classification.specificIntent = classifyTrackIntent(lowerInput);
        classification.domain = AudioProductionDomain::Composition;
        classification.confidence = 0.8;
        
        if (lowerInput.find("create") != std::string::npos) {
            classification.specificIntent = "track_create";
        } else if (lowerInput.find("mute") != std::string::npos) {
            classification.specificIntent = "track_mute";
        }
    }
    // Check query patterns
    else if (matchesPatterns(lowerInput, patterns::QUERY_PATTERNS)) {
        classification.type = IntentType::Query;
        classification.specificIntent = "query_information";
        classification.domain = AudioProductionDomain::Workflow;
        classification.confidence = 0.7;
    }
    // Check help patterns
    else if (matchesPatterns(lowerInput, patterns::HELP_PATTERNS)) {
        classification.type = IntentType::Help;
        classification.specificIntent = "help_request";
        classification.domain = AudioProductionDomain::Workflow;
        classification.confidence = 0.8;
    }
    else {
        classification.specificIntent = "unknown";
        classification.confidence = 0.1;
        classification.needsClarification = true;
    }
    
    classification.features = features;
    
    return classification;
}

bool IntentRecognition::matchesPatterns(const std::string& input, const std::vector<std::string>& patterns) const {
    for (const auto& pattern : patterns) {
        try {
            std::regex regex(pattern, std::regex::icase);
            if (std::regex_search(input, regex)) {
                return true;
            }
        } catch (const std::regex_error&) {
            // Skip invalid regex patterns
            continue;
        }
    }
    return false;
}

std::string IntentRecognition::classifyTransportIntent(const std::string& input) const {
    if (input.find("play") != std::string::npos) return "transport_play";
    if (input.find("stop") != std::string::npos) return "transport_stop";
    if (input.find("record") != std::string::npos) return "transport_record";
    if (input.find("tempo") != std::string::npos) return "transport_set_tempo";
    return "transport_unknown";
}

std::string IntentRecognition::classifyTrackIntent(const std::string& input) const {
    if (input.find("create") != std::string::npos) return "track_create";
    if (input.find("mute") != std::string::npos) return "track_mute";
    if (input.find("solo") != std::string::npos) return "track_solo";
    if (input.find("volume") != std::string::npos) return "track_volume";
    return "track_unknown";
}

std::vector<IntentClassification> IntentRecognition::getAllClassifications(
    const std::string& input, 
    const IntentRecognitionContext& context) const {
    
    std::vector<IntentClassification> classifications;
    
    // Get primary classification
    auto features = extractFeatures(input);
    auto primaryClassification = classifyUsingPatterns(input, features);
    classifications.push_back(primaryClassification);
    
    // Generate alternative classifications with lower confidence
    if (primaryClassification.confidence < 0.8) {
        // Add generic alternatives
        IntentClassification alt1;
        alt1.type = IntentType::Query;
        alt1.specificIntent = "general_query";
        alt1.confidence = 0.3;
        alt1.domain = AudioProductionDomain::Workflow;
        classifications.push_back(alt1);
        
        IntentClassification alt2;
        alt2.type = IntentType::Help;
        alt2.specificIntent = "help_request";
        alt2.confidence = 0.2;
        alt2.domain = AudioProductionDomain::Workflow;
        classifications.push_back(alt2);
    }
    
    return classifications;
}

void IntentRecognition::enrichWithContext(IntentClassification& classification, const IntentRecognitionContext& context) const {
    // Adjust confidence based on context
    if (!context.conversation.recentIntents.empty()) {
        // If this intent is similar to recent intents, boost confidence
        std::string lastIntent = context.conversation.recentIntents.back();
        if (classification.specificIntent.find(lastIntent.substr(0, lastIntent.find('_'))) != std::string::npos) {
            classification.confidence = std::min(1.0, classification.confidence + 0.1);
            classification.contextualConfidence = 0.8;
        }
    }
    
    // Domain expertise adjustment
    if (context.userExpertiseLevel == "beginner" && classification.domain == AudioProductionDomain::Technical) {
        classification.confidence *= 0.9; // Slightly reduce for beginners on technical topics
    }
}

std::vector<Entity> IntentRecognition::extractNumberEntities(const std::string& input) const {
    std::vector<Entity> entities;
    std::regex numberRegex(R"(\b(\d+(?:\.\d+)?)\b)");
    std::sregex_iterator iter(input.begin(), input.end(), numberRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        Entity entity;
        entity.type = EntityType::Number;
        entity.text = iter->str(1);
        entity.value = entity.text;
        entity.startPos = iter->position(1);
        entity.endPos = entity.startPos + entity.text.length();
        entity.confidence = 0.9;
        
        entities.push_back(entity);
    }
    
    return entities;
}

std::vector<Entity> IntentRecognition::extractTrackEntities(const std::string& input) const {
    std::vector<Entity> entities;
    std::regex trackRegex(R"(\b(track|channel)\s*(\d+)\b)", std::regex::icase);
    std::sregex_iterator iter(input.begin(), input.end(), trackRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        Entity entity;
        entity.type = EntityType::Track;
        entity.text = iter->str(0);
        entity.value = iter->str(2); // The number
        entity.startPos = iter->position(0);
        entity.endPos = entity.startPos + entity.text.length();
        entity.confidence = 0.95;
        
        entities.push_back(entity);
    }
    
    return entities;
}

std::vector<Entity> IntentRecognition::extractTimeEntities(const std::string& input) const {
    std::vector<Entity> entities;
    
    // Bar/measure references
    std::regex barRegex(R"(\b(bar|measure)\s*(\d+)\b)", std::regex::icase);
    std::sregex_iterator iter(input.begin(), input.end(), barRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        Entity entity;
        entity.type = EntityType::Time;
        entity.text = iter->str(0);
        entity.value = iter->str(2);
        entity.unit = "bar";
        entity.startPos = iter->position(0);
        entity.endPos = entity.startPos + entity.text.length();
        entity.confidence = 0.9;
        
        entities.push_back(entity);
    }
    
    return entities;
}

std::vector<Entity> IntentRecognition::extractAudioTermEntities(const std::string& input) const {
    std::vector<Entity> entities;
    
    // Common audio terms
    std::vector<std::string> audioTerms = {
        "compressor", "eq", "equalizer", "reverb", "delay", "chorus", "distortion",
        "gain", "volume", "pan", "mute", "solo", "bypass", "plugin", "effect"
    };
    
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    
    for (const auto& term : audioTerms) {
        size_t pos = lowerInput.find(term);
        if (pos != std::string::npos) {
            Entity entity;
            entity.type = EntityType::Parameter; // Generic for audio terms
            entity.text = term;
            entity.value = term;
            entity.startPos = pos;
            entity.endPos = pos + term.length();
            entity.confidence = 0.8;
            
            entities.push_back(entity);
        }
    }
    
    return entities;
}

bool IntentRecognition::isActionWord(const std::string& word) const {
    static const std::unordered_set<std::string> actionWords = {
        "play", "stop", "record", "create", "delete", "mute", "solo", "add", "remove",
        "set", "change", "adjust", "increase", "decrease", "start", "begin", "end",
        "save", "load", "open", "close", "cut", "copy", "paste", "split", "trim"
    };
    
    return actionWords.find(word) != actionWords.end();
}

bool IntentRecognition::isObjectWord(const std::string& word) const {
    static const std::unordered_set<std::string> objectWords = {
        "track", "channel", "clip", "plugin", "effect", "session", "project",
        "tempo", "volume", "pan", "eq", "compressor", "reverb", "delay"
    };
    
    return objectWords.find(word) != objectWords.end();
}

bool IntentRecognition::isAudioTerm(const std::string& word) const {
    static const std::unordered_set<std::string> audioTerms = {
        "audio", "midi", "sound", "music", "frequency", "amplitude", "phase",
        "stereo", "mono", "mix", "master", "bus", "send", "return", "insert",
        "bpm", "tempo", "beat", "bar", "measure", "quantize", "swing"
    };
    
    return audioTerms.find(word) != audioTerms.end();
}

void IntentRecognition::updateStats(const IntentClassification& classification) {
    std::lock_guard lock(statsMutex_);
    
    stats_.totalClassifications++;
    
    if (classification.confidence > 0.8) {
        stats_.successfulClassifications++;
    } else if (classification.needsClarification) {
        stats_.ambiguousClassifications++;
    } else {
        stats_.failedClassifications++;
    }
    
    stats_.averageConfidence = 
        (stats_.averageConfidence * (stats_.totalClassifications - 1) + classification.confidence) / 
        stats_.totalClassifications;
    
    stats_.intentDistribution[classification.specificIntent]++;
}

// ============================================================================
// Mock Component Implementations
// ============================================================================

class IntentRecognition::FeatureExtractor {
public:
    FeatureExtractor() = default;
    ~FeatureExtractor() = default;
};

class IntentRecognition::PatternMatcher {
public:
    PatternMatcher() = default;
    ~PatternMatcher() = default;
};

class IntentRecognition::EntityRecognizer {
public:
    EntityRecognizer() = default;
    ~EntityRecognizer() = default;
};

class IntentRecognition::ContextProcessor {
public:
    ContextProcessor() = default;
    ~ContextProcessor() = default;
};

class IntentRecognition::LearningEngine {
public:
    LearningEngine() = default;
    ~LearningEngine() = default;
};

// ============================================================================
// Global Intent Recognition Instance
// ============================================================================

IntentRecognition& getGlobalIntentRecognition() {
    static IntentRecognition instance;
    return instance;
}

} // namespace mixmind::ai