#include "ActionReducer.h"
#include <chrono>
#include <sstream>

namespace mixmind::ai {

    // ActionHistory implementation
    void ActionHistory::recordAction(const ActionCommand& action, const ProjectState& resultingState) {
        // If we're not at the end of history, truncate everything after current position
        if (currentIndex_ < history_.size()) {
            history_.erase(history_.begin() + currentIndex_, history_.end());
            stateHistory_.erase(stateHistory_.begin() + currentIndex_ + 1, stateHistory_.end());
        }
        
        // Add new action and state
        history_.push_back(action);
        stateHistory_.push_back(resultingState);
        currentIndex_ = history_.size();
        
        // Maintain maximum history size
        if (history_.size() > maxHistorySize_) {
            history_.erase(history_.begin());
            stateHistory_.erase(stateHistory_.begin());
            currentIndex_--;
        }
    }

    core::Result<ProjectState> ActionHistory::undo() {
        if (!canUndo()) {
            return core::Error{"No actions to undo"};
        }
        
        currentIndex_--;
        return stateHistory_[currentIndex_];
    }

    core::Result<ProjectState> ActionHistory::redo() {
        if (!canRedo()) {
            return core::Error{"No actions to redo"};
        }
        
        currentIndex_++;
        return stateHistory_[currentIndex_];
    }

    std::string ActionHistory::getUndoDescription() const {
        if (!canUndo()) return "";
        
        const auto& action = history_[currentIndex_ - 1];
        switch (action.type) {
            case ActionType::ADD_TRACK: return "Undo Add Track";
            case ActionType::REMOVE_TRACK: return "Undo Remove Track";
            case ActionType::RENAME_TRACK: return "Undo Rename Track";
            case ActionType::SET_TRACK_VOLUME: return "Undo Set Volume";
            case ActionType::SET_TRACK_PAN: return "Undo Set Pan";
            case ActionType::MUTE_TRACK: return "Undo Mute Track";
            case ActionType::SOLO_TRACK: return "Undo Solo Track";
            case ActionType::ADD_MIDI_NOTE: return "Undo Add MIDI Note";
            case ActionType::REMOVE_MIDI_NOTE: return "Undo Remove MIDI Note";
            case ActionType::SET_TEMPO: return "Undo Set Tempo";
            default: return "Undo Action";
        }
    }

    std::string ActionHistory::getRedoDescription() const {
        if (!canRedo()) return "";
        
        const auto& action = history_[currentIndex_];
        switch (action.type) {
            case ActionType::ADD_TRACK: return "Redo Add Track";
            case ActionType::REMOVE_TRACK: return "Redo Remove Track";
            case ActionType::RENAME_TRACK: return "Redo Rename Track";
            case ActionType::SET_TRACK_VOLUME: return "Redo Set Volume";
            case ActionType::SET_TRACK_PAN: return "Redo Set Pan";
            case ActionType::MUTE_TRACK: return "Redo Mute Track";
            case ActionType::SOLO_TRACK: return "Redo Solo Track";
            case ActionType::ADD_MIDI_NOTE: return "Redo Add MIDI Note";
            case ActionType::REMOVE_MIDI_NOTE: return "Redo Remove MIDI Note";
            case ActionType::SET_TEMPO: return "Redo Set Tempo";
            default: return "Redo Action";
        }
    }

    void ActionHistory::clear() {
        history_.clear();
        stateHistory_.clear();
        currentIndex_ = 0;
    }

    void ActionHistory::compress(size_t keepLastN) {
        if (history_.size() <= keepLastN) return;
        
        size_t removeCount = history_.size() - keepLastN;
        history_.erase(history_.begin(), history_.begin() + removeCount);
        stateHistory_.erase(stateHistory_.begin(), stateHistory_.begin() + removeCount);
        
        currentIndex_ = std::min(currentIndex_, history_.size());
    }

    // ActionPipeline implementation
    ActionPipeline::ActionPipeline() : history_(std::make_unique<ActionHistory>()) {
        // Initialize with empty project state
        currentState_.tempo = 120.0;
        currentState_.timeSignature = {4, 4};
        currentState_.keySignature = "C";
        currentState_.version = 1;
    }

    ActionPipeline::ActionPipeline(const ProjectState& initialState) 
        : currentState_(initialState), history_(std::make_unique<ActionHistory>()) {
        // Record initial state
        if (enableHistory_) {
            ActionCommand initialCmd; // Placeholder for initial state
            initialCmd.type = static_cast<ActionType>(-1); // Special marker
            history_->recordAction(initialCmd, currentState_);
        }
    }

    core::Result<ActionResult> ActionPipeline::executeAction(const ActionCommand& action) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Validate if enabled
        if (enableValidation_) {
            auto validation = ActionReducer::validateAction(currentState_, action);
            if (!validation.has_value()) {
                updateStats(false, 0.0);
                return core::Error{"Action validation failed: " + validation.error().message};
            }
        }
        
        // Execute action
        ActionResult result = ActionReducer::reduce(currentState_, action);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double executionTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        if (result.success) {
            currentState_ = result.newState;
            
            // Record in history if enabled
            if (enableHistory_) {
                recordAction(action, result);
            }
            
            updateStats(true, executionTime);
        } else {
            updateStats(false, executionTime);
        }
        
        return result;
    }

    core::Result<ActionResult> ActionPipeline::executeBatch(const std::vector<ActionCommand>& actions) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Validate all actions first if validation is enabled
        if (enableValidation_) {
            ProjectState tempState = currentState_;
            for (const auto& action : actions) {
                auto validation = ActionReducer::validateAction(tempState, action);
                if (!validation.has_value()) {
                    updateStats(false, 0.0);
                    return core::Error{"Batch validation failed at action: " + action.toString()};
                }
                // Simulate state changes for subsequent validations
                auto result = ActionReducer::reduce(tempState, action);
                if (!result.success) {
                    updateStats(false, 0.0);
                    return core::Error{"Batch simulation failed: " + result.errorMessage};
                }
                tempState = result.newState;
            }
        }
        
        // Execute batch
        ActionResult result = ActionReducer::reduceBatch(currentState_, actions);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double executionTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        if (result.success) {
            currentState_ = result.newState;
            
            // Record batch in history (simplified - each action recorded separately)
            if (enableHistory_) {
                ActionCommand batchCmd;
                batchCmd.type = static_cast<ActionType>(-2); // Special batch marker
                recordAction(batchCmd, result);
            }
            
            updateStats(true, executionTime);
        } else {
            updateStats(false, executionTime);
        }
        
        return result;
    }

    core::Result<ProjectState> ActionPipeline::undo() {
        if (!history_ || !enableHistory_) {
            return core::Error{"History not available"};
        }
        
        auto result = history_->undo();
        if (result.has_value()) {
            currentState_ = result.value();
            stats_.undoOperations++;
        }
        
        return result;
    }

    core::Result<ProjectState> ActionPipeline::redo() {
        if (!history_ || !enableHistory_) {
            return core::Error{"History not available"};
        }
        
        auto result = history_->redo();
        if (result.has_value()) {
            currentState_ = result.value();
            stats_.redoOperations++;
        }
        
        return result;
    }

    void ActionPipeline::setState(const ProjectState& newState) {
        currentState_ = newState;
        
        // Record state change in history if enabled
        if (enableHistory_) {
            ActionCommand stateChangeCmd;
            stateChangeCmd.type = static_cast<ActionType>(-3); // Special state change marker
            
            ActionResult fakeResult;
            fakeResult.success = true;
            fakeResult.newState = newState;
            
            recordAction(stateChangeCmd, fakeResult);
        }
    }

    void ActionPipeline::resetToInitialState() {
        ProjectState initialState;
        initialState.tempo = 120.0;
        initialState.timeSignature = {4, 4};
        initialState.keySignature = "C";
        initialState.version = 1;
        
        setState(initialState);
        
        if (history_) {
            history_->clear();
        }
        
        resetStats();
    }

    core::Result<void> ActionPipeline::validateCurrentState() const {
        if (!currentState_.validate()) {
            return core::Error{"Current project state is invalid"};
        }
        return {};
    }

    std::string ActionPipeline::serialize() const {
        std::ostringstream json;
        json << "{\\n";
        json << "  \\"currentState\\": " << currentState_.toJSON() << ",\\n";
        json << "  \\"stats\\": {\\n";
        json << "    \\"totalActionsExecuted\\": " << stats_.totalActionsExecuted << ",\\n";
        json << "    \\"successfulActions\\": " << stats_.successfulActions << ",\\n";
        json << "    \\"failedActions\\": " << stats_.failedActions << ",\\n";
        json << "    \\"undoOperations\\": " << stats_.undoOperations << ",\\n";
        json << "    \\"redoOperations\\": " << stats_.redoOperations << ",\\n";
        json << "    \\"averageExecutionTime_ms\\": " << stats_.averageExecutionTime_ms << "\\n";
        json << "  },\\n";
        json << "  \\"historySize\\": " << (history_ ? history_->getHistorySize() : 0) << ",\\n";
        json << "  \\"canUndo\\": " << (canUndo() ? "true" : "false") << ",\\n";
        json << "  \\"canRedo\\": " << (canRedo() ? "true" : "false") << "\\n";
        json << "}";
        
        return json.str();
    }

    core::Result<void> ActionPipeline::deserialize(const std::string& data) {
        // Simplified deserialization - in real implementation would parse JSON
        // For now, just validate that it looks like valid JSON
        if (data.empty() || data[0] != '{' || data.back() != '}') {
            return core::Error{"Invalid serialization data format"};
        }
        
        // In a real implementation, this would:
        // 1. Parse JSON to extract currentState
        // 2. Restore history if included
        // 3. Restore statistics
        // 4. Validate the restored state
        
        return {};
    }

    void ActionPipeline::updateStats(bool success, double executionTime_ms) {
        stats_.totalActionsExecuted++;
        
        if (success) {
            stats_.successfulActions++;
        } else {
            stats_.failedActions++;
        }
        
        // Update rolling average execution time
        double totalTime = stats_.averageExecutionTime_ms * (stats_.totalActionsExecuted - 1);
        stats_.averageExecutionTime_ms = (totalTime + executionTime_ms) / stats_.totalActionsExecuted;
    }

    void ActionPipeline::recordAction(const ActionCommand& action, const ActionResult& result) {
        if (history_ && enableHistory_) {
            history_->recordAction(action, result.newState);
        }
    }

} // namespace mixmind::ai