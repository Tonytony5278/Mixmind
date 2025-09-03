#pragma once

#include "types.h"
#include "result.h"
#include <memory>
#include <string>
#include <vector>

namespace mixmind::core {

// Forward declarations
class ITrack;
class ITransport;
class IPluginHost;
class IAutomation;
class IRenderService;
class IMediaLibrary;

// ============================================================================
// Session Interface - Top-level DAW session management
// ============================================================================

class ISession {
public:
    virtual ~ISession() = default;
    
    // ========================================================================
    // Session Lifecycle
    // ========================================================================
    
    /// Create a new empty session
    virtual AsyncResult<VoidResult> createNew(const std::string& name, const AudioConfig& config) = 0;
    
    /// Load session from file
    virtual AsyncResult<VoidResult> loadFromFile(const std::string& filePath) = 0;
    
    /// Save session to file  
    virtual AsyncResult<VoidResult> saveToFile(const std::string& filePath) = 0;
    
    /// Save session (use current file path)
    virtual AsyncResult<VoidResult> save() = 0;
    
    /// Close current session
    virtual AsyncResult<VoidResult> close() = 0;
    
    // ========================================================================
    // Session Properties
    // ========================================================================
    
    /// Get session ID
    virtual SessionID getId() const = 0;
    
    /// Get session name
    virtual std::string getName() const = 0;
    
    /// Set session name
    virtual VoidResult setName(const std::string& name) = 0;
    
    /// Get current file path (empty if not saved yet)
    virtual std::string getFilePath() const = 0;
    
    /// Check if session has unsaved changes
    virtual bool hasUnsavedChanges() const = 0;
    
    /// Get audio configuration
    virtual AudioConfig getAudioConfig() const = 0;
    
    /// Update audio configuration
    virtual AsyncResult<VoidResult> setAudioConfig(const AudioConfig& config) = 0;
    
    // ========================================================================
    // Track Management
    // ========================================================================
    
    /// Create new track
    virtual AsyncResult<Result<TrackID>> createTrack(const TrackConfig& config) = 0;
    
    /// Delete track
    virtual AsyncResult<VoidResult> deleteTrack(TrackID trackId) = 0;
    
    /// Get track by ID
    virtual std::shared_ptr<ITrack> getTrack(TrackID trackId) = 0;
    
    /// Get all tracks
    virtual std::vector<std::shared_ptr<ITrack>> getAllTracks() = 0;
    
    /// Get number of tracks
    virtual int32_t getTrackCount() const = 0;
    
    /// Reorder tracks
    virtual VoidResult moveTrack(TrackID trackId, int32_t newIndex) = 0;
    
    /// Duplicate track
    virtual AsyncResult<Result<TrackID>> duplicateTrack(TrackID trackId, const std::string& newName = "") = 0;
    
    // ========================================================================
    // Audio Import/Export
    // ========================================================================
    
    /// Import audio files into session
    virtual AsyncResult<Result<std::vector<ClipID>>> importAudio(const ImportConfig& config) = 0;
    
    /// Import MIDI files into session  
    virtual AsyncResult<Result<std::vector<ClipID>>> importMIDI(const ImportConfig& config) = 0;
    
    /// Export entire session or selection
    virtual AsyncResult<VoidResult> exportAudio(const RenderSettings& settings) = 0;
    
    // ========================================================================
    // Transport Integration
    // ========================================================================
    
    /// Get transport interface
    virtual std::shared_ptr<ITransport> getTransport() = 0;
    
    // ========================================================================
    // Plugin Management
    // ========================================================================
    
    /// Get plugin host interface
    virtual std::shared_ptr<IPluginHost> getPluginHost() = 0;
    
    // ========================================================================
    // Automation
    // ========================================================================
    
    /// Get automation interface
    virtual std::shared_ptr<IAutomation> getAutomation() = 0;
    
    // ========================================================================
    // Rendering
    // ========================================================================
    
    /// Get render service interface
    virtual std::shared_ptr<IRenderService> getRenderService() = 0;
    
    // ========================================================================
    // Media Management
    // ========================================================================
    
    /// Get media library interface
    virtual std::shared_ptr<IMediaLibrary> getMediaLibrary() = 0;
    
    // ========================================================================
    // Time and Tempo
    // ========================================================================
    
    /// Get tempo at specific position
    virtual double getTempoAtPosition(TimestampSamples position) const = 0;
    
    /// Set tempo (constant throughout session)
    virtual VoidResult setTempo(double beatsPerMinute) = 0;
    
    /// Get tempo map (for variable tempo)
    virtual TempoMap getTempoMap() const = 0;
    
    /// Set tempo map
    virtual VoidResult setTempoMap(const TempoMap& tempoMap) = 0;
    
    /// Get time signature
    virtual TimeSignature getTimeSignature() const = 0;
    
    /// Set time signature
    virtual VoidResult setTimeSignature(const TimeSignature& timeSignature) = 0;
    
    /// Convert samples to musical time (bars:beats:ticks)
    virtual std::string samplesToMusicalTime(TimestampSamples samples) const = 0;
    
    /// Convert musical time to samples
    virtual TimestampSamples musicalTimeToSamples(const std::string& musicalTime) const = 0;
    
    // ========================================================================
    // Session Length and Navigation
    // ========================================================================
    
    /// Get total session length in samples
    virtual TimestampSamples getLength() const = 0;
    
    /// Get session start time (usually 0, but can be offset)
    virtual TimestampSamples getStartTime() const = 0;
    
    /// Set session start time
    virtual VoidResult setStartTime(TimestampSamples startTime) = 0;
    
    // ========================================================================
    // Markers and Regions
    // ========================================================================
    
    struct Marker {
        std::string name;
        TimestampSamples position;
        std::string color;
        bool isRegionStart = false;
        TimestampSamples regionEnd = 0;  // Only valid if isRegionStart is true
    };
    
    /// Add marker
    virtual VoidResult addMarker(const Marker& marker) = 0;
    
    /// Remove marker
    virtual VoidResult removeMarker(const std::string& name) = 0;
    
    /// Get all markers
    virtual std::vector<Marker> getMarkers() const = 0;
    
    /// Find marker at position
    virtual std::optional<Marker> getMarkerAtPosition(TimestampSamples position) const = 0;
    
    // ========================================================================
    // Undo/Redo System
    // ========================================================================
    
    /// Check if undo is available
    virtual bool canUndo() const = 0;
    
    /// Check if redo is available  
    virtual bool canRedo() const = 0;
    
    /// Perform undo
    virtual AsyncResult<VoidResult> undo() = 0;
    
    /// Perform redo
    virtual AsyncResult<VoidResult> redo() = 0;
    
    /// Get undo history description
    virtual std::vector<std::string> getUndoHistory() const = 0;
    
    /// Get redo history description
    virtual std::vector<std::string> getRedoHistory() const = 0;
    
    /// Clear undo/redo history
    virtual VoidResult clearHistory() = 0;
    
    // ========================================================================
    // Transactional Operations
    // ========================================================================
    
    /// Begin transaction for batch operations
    virtual TransactionPtr beginTransaction() = 0;
    
    /// Create dry-run transaction (preview changes without applying)
    virtual AsyncResult<Result<MixedDiff>> dryRun(std::function<void()> operation) = 0;
    
    // ========================================================================
    // Session Metadata
    // ========================================================================
    
    struct SessionMetadata {
        std::string title;
        std::string artist;
        std::string album;
        std::string genre;
        int32_t year = 0;
        std::string comments;
        std::string copyright;
        std::vector<std::string> tags;
        
        // Technical metadata
        std::chrono::system_clock::time_point createdDate;
        std::chrono::system_clock::time_point modifiedDate;
        std::string createdBy;
        std::string lastModifiedBy;
        std::string applicationVersion;
    };
    
    /// Get session metadata
    virtual SessionMetadata getMetadata() const = 0;
    
    /// Set session metadata
    virtual VoidResult setMetadata(const SessionMetadata& metadata) = 0;
    
    // ========================================================================
    // Session Statistics
    // ========================================================================
    
    struct SessionStats {
        int32_t trackCount = 0;
        int32_t audioClipCount = 0;
        int32_t midiClipCount = 0;
        int32_t pluginCount = 0;
        TimestampSamples totalLength = 0;
        size_t estimatedMemoryUsage = 0;  // bytes
        size_t diskUsage = 0;  // bytes
        double cpuUsage = 0.0;  // percentage
    };
    
    /// Get session statistics
    virtual SessionStats getStats() const = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class SessionEvent {
        TrackAdded,
        TrackRemoved, 
        TrackMoved,
        ClipAdded,
        ClipRemoved,
        ClipMoved,
        TempoChanged,
        TimeSignatureChanged,
        MarkerAdded,
        MarkerRemoved,
        MetadataChanged,
        AudioConfigChanged
    };
    
    using SessionEventCallback = std::function<void(SessionEvent event, const std::string& details)>;
    
    /// Subscribe to session events
    virtual void addEventListener(SessionEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(SessionEventCallback callback) = 0;
    
    // ========================================================================
    // Session Validation
    // ========================================================================
    
    struct ValidationIssue {
        Severity severity;
        std::string description;
        std::string suggestion;
        std::string location;  // e.g., "track[2]/clip[0]"
    };
    
    /// Validate session integrity
    virtual AsyncResult<Result<std::vector<ValidationIssue>>> validateSession() = 0;
    
    /// Attempt to fix validation issues automatically
    virtual AsyncResult<VoidResult> autoFix() = 0;
};

// ============================================================================
// Session Factory Interface
// ============================================================================

class ISessionFactory {
public:
    virtual ~ISessionFactory() = default;
    
    /// Create new session instance
    virtual std::shared_ptr<ISession> createSession() = 0;
    
    /// Check if file is a valid session file
    virtual bool isValidSessionFile(const std::string& filePath) = 0;
    
    /// Get supported session file extensions
    virtual std::vector<std::string> getSupportedExtensions() = 0;
    
    /// Get session file format version
    virtual std::string getFormatVersion() = 0;
};

} // namespace mixmind::core