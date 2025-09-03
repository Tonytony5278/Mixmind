#pragma once

#include "../../core/ISession.h"
#include "TEAdapter.h"
#include <tracktion_engine/tracktion_engine.h>
#include <memory>
#include <unordered_map>

namespace mixmind::adapters::tracktion {

// ============================================================================
// TE Session Adapter - Implements ISession using Tracktion Engine Edit
// ============================================================================

class TESession : public core::ISession, public TEAdapter {
public:
    explicit TESession(te::Engine& engine);
    explicit TESession(te::Engine& engine, std::unique_ptr<te::Edit> edit);
    ~TESession() override;
    
    // ========================================================================
    // ISession Implementation - Session Management
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> createNewSession(const core::SessionConfig& config) override;
    core::AsyncResult<core::VoidResult> loadSession(const std::string& filePath) override;
    core::AsyncResult<core::VoidResult> saveSession() override;
    core::AsyncResult<core::VoidResult> saveSessionAs(const std::string& filePath) override;
    core::AsyncResult<core::VoidResult> closeSession() override;
    
    bool hasUnsavedChanges() const override;
    std::string getCurrentFilePath() const override;
    core::SessionInfo getSessionInfo() const override;
    
    // ========================================================================
    // ISession Implementation - Session Properties
    // ========================================================================
    
    std::string getSessionName() const override;
    core::AsyncResult<core::VoidResult> setSessionName(const std::string& name) override;
    
    std::string getArtist() const override;
    core::AsyncResult<core::VoidResult> setArtist(const std::string& artist) override;
    
    std::string getComments() const override;
    core::AsyncResult<core::VoidResult> setComments(const std::string& comments) override;
    
    core::SampleRate getSampleRate() const override;
    core::AsyncResult<core::VoidResult> setSampleRate(core::SampleRate sampleRate) override;
    
    core::BufferSize getBufferSize() const override;
    core::AsyncResult<core::VoidResult> setBufferSize(core::BufferSize bufferSize) override;
    
    float getTempo() const override;
    core::AsyncResult<core::VoidResult> setTempo(float bpm) override;
    
    core::TimeSignature getTimeSignature() const override;
    core::AsyncResult<core::VoidResult> setTimeSignature(const core::TimeSignature& timeSig) override;
    
    std::string getMusicalKey() const override;
    core::AsyncResult<core::VoidResult> setMusicalKey(const std::string& key) override;
    
    // ========================================================================
    // ISession Implementation - Track Management
    // ========================================================================
    
    core::AsyncResult<core::Result<core::TrackID>> createAudioTrack(const std::string& name) override;
    core::AsyncResult<core::Result<core::TrackID>> createMIDITrack(const std::string& name) override;
    core::AsyncResult<core::Result<core::TrackID>> createFolderTrack(const std::string& name) override;
    core::AsyncResult<core::VoidResult> deleteTrack(core::TrackID trackId) override;
    core::AsyncResult<core::Result<core::TrackID>> duplicateTrack(core::TrackID trackId) override;
    
    std::shared_ptr<core::ITrack> getTrack(core::TrackID trackId) override;
    std::vector<std::shared_ptr<core::ITrack>> getAllTracks() override;
    std::vector<std::shared_ptr<core::ITrack>> getSelectedTracks() override;
    
    core::AsyncResult<core::VoidResult> selectTrack(core::TrackID trackId, bool selected) override;
    core::AsyncResult<core::VoidResult> selectAllTracks() override;
    core::AsyncResult<core::VoidResult> clearTrackSelection() override;
    
    core::AsyncResult<core::VoidResult> moveTrack(core::TrackID trackId, int32_t newPosition) override;
    core::AsyncResult<core::VoidResult> groupTracks(const std::vector<core::TrackID>& trackIds, const std::string& groupName) override;
    core::AsyncResult<core::VoidResult> ungroupTracks(const std::vector<core::TrackID>& trackIds) override;
    
    int32_t getTrackCount() const override;
    
    // ========================================================================
    // ISession Implementation - Audio/MIDI Import
    // ========================================================================
    
    core::AsyncResult<core::Result<core::ImportResult>> importAudio(const core::ImportConfig& config) override;
    core::AsyncResult<core::Result<core::ImportResult>> importMIDI(const core::ImportConfig& config) override;
    core::AsyncResult<core::Result<std::vector<core::ImportResult>>> importMultipleFiles(const std::vector<core::ImportConfig>& configs) override;
    
    // ========================================================================
    // ISession Implementation - Transport Access
    // ========================================================================
    
    std::shared_ptr<core::ITransport> getTransport() override;
    
    // ========================================================================
    // ISession Implementation - Undo/Redo
    // ========================================================================
    
    bool canUndo() const override;
    bool canRedo() const override;
    std::string getUndoDescription() const override;
    std::string getRedoDescription() const override;
    
    core::AsyncResult<core::VoidResult> undo() override;
    core::AsyncResult<core::VoidResult> redo() override;
    core::AsyncResult<core::VoidResult> clearUndoHistory() override;
    
    // ========================================================================
    // ISession Implementation - Session State
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> saveState(std::vector<uint8_t>& data) const override;
    core::AsyncResult<core::VoidResult> loadState(const std::vector<uint8_t>& data) override;
    
    core::SessionValidationResult validateSession() const override;
    core::AsyncResult<core::VoidResult> repairSession(const std::vector<core::SessionIssue>& issuesToFix) override;
    
    // ========================================================================
    // ISession Implementation - Events
    // ========================================================================
    
    void addEventListener(core::SessionEventCallback callback) override;
    void removeEventListener(core::SessionEventCallback callback) override;
    
    // ========================================================================
    // TE-Specific Methods
    // ========================================================================
    
    /// Get the underlying Tracktion Engine Edit
    te::Edit& getEdit() { return *edit_; }
    const te::Edit& getEdit() const { return *edit_; }
    
    /// Check if edit is valid
    bool isEditValid() const { return edit_ != nullptr; }
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    void initializeEdit(const core::SessionConfig& config);
    void setupDefaultTracks();
    void updateSessionInfo();
    void notifySessionChanged(const std::string& change);
    
    /// Convert TE track to our track wrapper
    std::shared_ptr<core::ITrack> wrapTrack(te::Track::Ptr teTrack);
    
    /// Find TE track by our track ID
    te::Track::Ptr findTETrack(core::TrackID trackId) const;
    
    /// Generate unique track ID
    core::TrackID generateTrackID() const;
    
    /// Import audio file to track
    core::AsyncResult<core::Result<core::ImportResult>> importAudioToTrack(
        const std::string& filePath, 
        te::Track::Ptr track, 
        core::TimestampSamples position
    );
    
    /// Import MIDI file to track
    core::AsyncResult<core::Result<core::ImportResult>> importMIDIToTrack(
        const std::string& filePath, 
        te::Track::Ptr track, 
        core::TimestampSamples position
    );
    
    /// Update tempo map from TE
    void updateTempoMap();
    
    /// Setup engine callbacks
    void setupEngineCallbacks();
    void cleanupEngineCallbacks();
    
private:
    std::unique_ptr<te::Edit> edit_;
    std::shared_ptr<core::ITransport> transport_;
    
    // Track mapping
    std::unordered_map<core::TrackID, te::Track::Ptr> trackMap_;
    std::unordered_map<te::Track*, core::TrackID> reverseTrackMap_;
    mutable std::atomic<uint32_t> nextTrackId_{1};
    
    // Session state
    std::string currentFilePath_;
    mutable std::recursive_mutex sessionMutex_;
    std::atomic<bool> hasUnsavedChanges_{false};
    
    // Event callbacks
    std::vector<core::SessionEventCallback> eventCallbacks_;
    std::mutex callbackMutex_;
    
    // TE-specific state
    std::unique_ptr<te::EditPlaybackContext> playbackContext_;
    std::unique_ptr<te::TransportControl> transportControl_;
    
    // Cached session info
    mutable core::SessionInfo cachedSessionInfo_;
    mutable std::chrono::steady_clock::time_point lastInfoUpdate_;
    
    static constexpr auto INFO_CACHE_DURATION = std::chrono::milliseconds(100);
};

} // namespace mixmind::adapters::tracktion