#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <functional>
#include "../core/result.h"

namespace mixmind::ai {

    // Forward declarations
    struct ProjectState;
    struct ActionCommand;

    // Action types for deterministic AI operations
    enum class ActionType {
        // Track management
        ADD_TRACK,
        REMOVE_TRACK,
        RENAME_TRACK,
        SET_TRACK_VOLUME,
        SET_TRACK_PAN,
        MUTE_TRACK,
        SOLO_TRACK,
        
        // MIDI operations
        ADD_MIDI_NOTE,
        REMOVE_MIDI_NOTE,
        MODIFY_MIDI_NOTE,
        QUANTIZE_MIDI,
        TRANSPOSE_MIDI,
        
        // Audio processing
        APPLY_EFFECT,
        REMOVE_EFFECT,
        MODIFY_EFFECT_PARAM,
        
        // Project-level operations
        SET_TEMPO,
        SET_TIME_SIGNATURE,
        SET_KEY_SIGNATURE,
        
        // Arrangement
        COPY_REGION,
        MOVE_REGION,
        DELETE_REGION,
        SPLIT_REGION,
        
        // Undo/Redo system
        UNDO,
        REDO
    };

    // Individual action parameters using variant for type safety
    using ActionParam = std::variant<
        int32_t,
        float,
        double,
        std::string,
        bool
    >;

    // Action command structure
    struct ActionCommand {
        ActionType type;
        std::vector<ActionParam> params;
        std::string trackId = "";
        std::string regionId = "";
        uint64_t timestamp_ms = 0;  // For deterministic playback
        
        // Validation helpers
        bool validate() const;
        std::string toString() const;
        
        // Parameter accessors with type checking
        template<typename T>
        core::Result<T> getParam(size_t index) const {
            if (index >= params.size()) {
                return core::Error{"Parameter index out of range"};
            }
            
            if (std::holds_alternative<T>(params[index])) {
                return std::get<T>(params[index]);
            }
            
            return core::Error{"Parameter type mismatch"};
        }
    };

    // Project state representation for deterministic operations
    struct ProjectState {
        struct Track {
            std::string id;
            std::string name;
            float volume = 1.0f;
            float pan = 0.0f;
            bool muted = false;
            bool soloed = false;
            std::vector<std::string> effectIds;
        };
        
        struct MIDINote {
            int32_t pitch = 60;
            float velocity = 0.8f;
            uint64_t startTime_ms = 0;
            uint64_t duration_ms = 500;
            std::string trackId;
        };
        
        struct AudioRegion {
            std::string id;
            std::string trackId;
            uint64_t startTime_ms = 0;
            uint64_t duration_ms = 1000;
            std::string audioFile = "";
        };
        
        // Project-level state
        double tempo = 120.0;
        std::pair<int, int> timeSignature = {4, 4};
        std::string keySignature = "C";
        
        // Collections
        std::vector<Track> tracks;
        std::vector<MIDINote> midiNotes;
        std::vector<AudioRegion> audioRegions;
        
        // State management
        uint64_t version = 0;
        std::string lastModified = "";
        
        // Deep copy for immutable operations
        ProjectState copy() const;
        
        // Validation
        bool validate() const;
        
        // Serialization
        std::string toJSON() const;
        static core::Result<ProjectState> fromJSON(const std::string& json);
    };

    // Action result for deterministic validation
    struct ActionResult {
        bool success = false;
        std::string errorMessage = "";
        ProjectState newState;
        std::vector<std::string> warnings;
        
        // For undo operations
        ActionCommand reverseCommand;
        
        bool isValid() const { return success && newState.validate(); }
    };

    // Pure functional action reducer (core of the system)
    class ActionReducer {
    public:
        // Main reduction function - pure and deterministic
        static ActionResult reduce(const ProjectState& currentState, const ActionCommand& action);
        
        // Batch action processing with transaction semantics
        static ActionResult reduceBatch(const ProjectState& currentState, 
                                       const std::vector<ActionCommand>& actions);
        
        // Action validation without execution
        static core::Result<void> validateAction(const ProjectState& currentState, 
                                                const ActionCommand& action);
        
        // Generate reverse command for undo functionality
        static core::Result<ActionCommand> generateReverseCommand(const ProjectState& beforeState,
                                                                 const ActionCommand& action);
        
    private:
        // Individual action handlers - all pure functions
        static ActionResult handleAddTrack(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleRemoveTrack(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleRenameTrack(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleSetTrackVolume(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleSetTrackPan(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleMuteTrack(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleSoloTrack(const ProjectState& state, const ActionCommand& cmd);
        
        static ActionResult handleAddMIDINote(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleRemoveMIDINote(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleModifyMIDINote(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleQuantizeMIDI(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleTransposeMIDI(const ProjectState& state, const ActionCommand& cmd);
        
        static ActionResult handleSetTempo(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleSetTimeSignature(const ProjectState& state, const ActionCommand& cmd);
        static ActionResult handleSetKeySignature(const ProjectState& state, const ActionCommand& cmd);
        
        // Helper functions
        static std::string generateId(const std::string& prefix = "");
        static uint64_t getCurrentTimestamp();
        static ProjectState::Track* findTrack(ProjectState& state, const std::string& trackId);
        static const ProjectState::Track* findTrack(const ProjectState& state, const std::string& trackId);
    };

    // Action history manager for undo/redo
    class ActionHistory {
    private:
        std::vector<ActionCommand> history_;
        std::vector<ProjectState> stateHistory_;
        size_t currentIndex_ = 0;
        size_t maxHistorySize_ = 1000;
        
    public:
        explicit ActionHistory(size_t maxSize = 1000) : maxHistorySize_(maxSize) {}
        
        // Record an action and resulting state
        void recordAction(const ActionCommand& action, const ProjectState& resultingState);
        
        // Undo/Redo operations
        core::Result<ProjectState> undo();
        core::Result<ProjectState> redo();
        
        // History queries
        bool canUndo() const { return currentIndex_ > 0; }
        bool canRedo() const { return currentIndex_ < history_.size(); }
        
        size_t getHistorySize() const { return history_.size(); }
        size_t getCurrentIndex() const { return currentIndex_; }
        
        // Get action description for UI
        std::string getUndoDescription() const;
        std::string getRedoDescription() const;
        
        // Clear history (for new projects)
        void clear();
        
        // Compress history (remove old entries)
        void compress(size_t keepLastN = 100);
    };

    // AI Action Pipeline - orchestrates the entire deterministic AI system
    class ActionPipeline {
    private:
        ProjectState currentState_;
        std::unique_ptr<ActionHistory> history_;
        
        // Configuration
        bool enableValidation_ = true;
        bool enableHistory_ = true;
        
    public:
        ActionPipeline();
        explicit ActionPipeline(const ProjectState& initialState);
        ~ActionPipeline() = default;
        
        // Main action execution pipeline
        core::Result<ActionResult> executeAction(const ActionCommand& action);
        core::Result<ActionResult> executeBatch(const std::vector<ActionCommand>& actions);
        
        // State access
        const ProjectState& getCurrentState() const { return currentState_; }
        ProjectState getCurrentStateCopy() const { return currentState_.copy(); }
        
        // Undo/Redo
        core::Result<ProjectState> undo();
        core::Result<ProjectState> redo();
        bool canUndo() const { return history_ && history_->canUndo(); }
        bool canRedo() const { return history_ && history_->canRedo(); }
        
        // Configuration
        void enableValidation(bool enable) { enableValidation_ = enable; }
        void enableHistory(bool enable) { enableHistory_ = enable; }
        
        // State management
        void setState(const ProjectState& newState);
        void resetToInitialState();
        
        // Validation
        core::Result<void> validateCurrentState() const;
        
        // Serialization for persistence
        std::string serialize() const;
        core::Result<void> deserialize(const std::string& data);
        
        // Statistics for monitoring
        struct PipelineStats {
            size_t totalActionsExecuted = 0;
            size_t successfulActions = 0;
            size_t failedActions = 0;
            size_t undoOperations = 0;
            size_t redoOperations = 0;
            double averageExecutionTime_ms = 0.0;
        };
        
        PipelineStats getStats() const { return stats_; }
        void resetStats() { stats_ = {}; }
        
    private:
        mutable PipelineStats stats_;
        
        void updateStats(bool success, double executionTime_ms);
        void recordAction(const ActionCommand& action, const ActionResult& result);
    };

} // namespace mixmind::ai