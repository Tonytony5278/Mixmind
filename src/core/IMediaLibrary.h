#pragma once

#include "types.h"
#include "result.h"
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <unordered_map>

namespace mixmind::core {

// ============================================================================
// Media Library Interface - Media file discovery and management
// ============================================================================

class IMediaLibrary {
public:
    virtual ~IMediaLibrary() = default;
    
    // ========================================================================
    // Media Item Types and Information
    // ========================================================================
    
    enum class MediaType {
        Audio,
        MIDI,
        Video,
        Image,
        Project,
        Unknown
    };
    
    enum class AudioFormat {
        WAV,
        FLAC,
        MP3,
        AAC,
        OGG,
        AIFF,
        M4A,
        WMA,
        REX,
        ACID
    };
    
    struct MediaInfo {
        std::string mediaId;
        std::string filePath;
        std::string filename;
        std::string directory;
        MediaType type;
        AudioFormat audioFormat;  // Only valid for audio files
        
        // File properties
        size_t fileSize = 0;      // bytes
        std::chrono::system_clock::time_point dateCreated;
        std::chrono::system_clock::time_point dateModified;
        std::chrono::system_clock::time_point dateAdded;
        
        // Audio properties
        SampleRate sampleRate = 0;
        int32_t bitDepth = 0;
        int32_t channels = 0;
        TimestampSamples lengthSamples = 0;
        double lengthSeconds = 0.0;
        float peakLevel = 0.0f;   // dB
        float rmsLevel = 0.0f;    // dB
        
        // Musical properties
        float bpm = 0.0f;
        std::string key;          // Musical key (C, C#, Dm, etc.)
        std::string timeSignature;
        
        // Metadata
        std::string title;
        std::string artist;
        std::string album;
        std::string genre;
        std::string comment;
        int32_t year = 0;
        int32_t trackNumber = 0;
        
        // Library organization
        std::vector<std::string> tags;
        std::vector<std::string> collections;
        float userRating = 0.0f;  // 0.0-5.0
        int32_t playCount = 0;
        bool favorite = false;
        
        // Analysis data
        std::string waveformData;     // Base64 encoded waveform
        std::string spectrumData;     // Base64 encoded spectrum
        std::vector<float> beatMarkers; // Beat positions in seconds
        std::vector<float> chordMarkers; // Chord change positions
    };
    
    // ========================================================================
    // Library Scanning and Discovery
    // ========================================================================
    
    /// Scan directory for media files
    virtual AsyncResult<Result<int32_t>> scanDirectory(
        const std::string& directoryPath,
        bool recursive = true,
        bool includeSubdirectories = true,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Scan multiple directories
    virtual AsyncResult<Result<int32_t>> scanDirectories(
        const std::vector<std::string>& directoryPaths,
        bool recursive = true,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Add individual media file
    virtual AsyncResult<Result<std::string>> addMediaFile(const std::string& filePath) = 0;
    
    /// Remove media file from library
    virtual AsyncResult<VoidResult> removeMediaFile(const std::string& mediaId) = 0;
    
    /// Refresh media file (re-scan metadata)
    virtual AsyncResult<VoidResult> refreshMediaFile(const std::string& mediaId) = 0;
    
    /// Get scan progress
    struct ScanProgress {
        int32_t totalFiles = 0;
        int32_t scannedFiles = 0;
        int32_t addedFiles = 0;
        int32_t skippedFiles = 0;
        std::string currentFile;
        bool isComplete = false;
    };
    
    virtual ScanProgress getScanProgress() const = 0;
    
    /// Cancel ongoing scan
    virtual VoidResult cancelScan() = 0;
    
    /// Check if scan is in progress
    virtual bool isScanInProgress() const = 0;
    
    // ========================================================================
    // Media Information and Metadata
    // ========================================================================
    
    /// Get media information by ID
    virtual std::optional<MediaInfo> getMediaInfo(const std::string& mediaId) const = 0;
    
    /// Get media information by file path
    virtual std::optional<MediaInfo> getMediaInfoByPath(const std::string& filePath) const = 0;
    
    /// Update media metadata
    virtual AsyncResult<VoidResult> updateMediaInfo(const std::string& mediaId, const MediaInfo& info) = 0;
    
    /// Get all media items
    virtual std::vector<MediaInfo> getAllMedia() const = 0;
    
    /// Get media count
    virtual int32_t getMediaCount() const = 0;
    
    /// Get media count by type
    virtual std::unordered_map<MediaType, int32_t> getMediaCountByType() const = 0;
    
    // ========================================================================
    // Search and Filtering
    // ========================================================================
    
    struct SearchFilter {
        std::string searchText;           // Free text search
        std::vector<MediaType> mediaTypes;
        std::vector<AudioFormat> audioFormats;
        
        // Duration filters
        double minDurationSeconds = 0.0;
        double maxDurationSeconds = 0.0;
        
        // BPM filters
        float minBPM = 0.0f;
        float maxBPM = 0.0f;
        
        // Key filters
        std::vector<std::string> keys;
        
        // Rating filters
        float minRating = 0.0f;
        float maxRating = 5.0f;
        
        // Date filters
        std::chrono::system_clock::time_point fromDate;
        std::chrono::system_clock::time_point toDate;
        
        // Tag filters
        std::vector<std::string> includeTags;
        std::vector<std::string> excludeTags;
        
        // Collection filters
        std::vector<std::string> collections;
        
        // Audio property filters
        int32_t minChannels = 0;
        int32_t maxChannels = 0;
        SampleRate minSampleRate = 0;
        SampleRate maxSampleRate = 0;
        int32_t minBitDepth = 0;
        int32_t maxBitDepth = 0;
        
        // File size filters
        size_t minFileSize = 0;  // bytes
        size_t maxFileSize = 0;  // bytes
        
        bool favoritesOnly = false;
        
        // Sort options
        enum class SortBy {
            Filename,
            DateAdded,
            DateModified,
            Duration,
            BPM,
            Rating,
            PlayCount,
            Artist,
            Title,
            FileSize
        } sortBy = SortBy::DateAdded;
        
        bool sortDescending = true;
    };
    
    /// Search media with filters
    virtual std::vector<MediaInfo> searchMedia(const SearchFilter& filter) const = 0;
    
    /// Get similar media items
    virtual std::vector<MediaInfo> findSimilarMedia(
        const std::string& mediaId,
        int32_t maxResults = 10
    ) const = 0;
    
    /// Find media by BPM range
    virtual std::vector<MediaInfo> findMediaByBPM(float minBPM, float maxBPM) const = 0;
    
    /// Find media by key
    virtual std::vector<MediaInfo> findMediaByKey(const std::string& key) const = 0;
    
    /// Get duplicate media files
    virtual std::vector<std::vector<MediaInfo>> findDuplicates() const = 0;
    
    // ========================================================================
    // Tags and Organization
    // ========================================================================
    
    /// Add tag to media item
    virtual AsyncResult<VoidResult> addTag(const std::string& mediaId, const std::string& tag) = 0;
    
    /// Remove tag from media item
    virtual AsyncResult<VoidResult> removeTag(const std::string& mediaId, const std::string& tag) = 0;
    
    /// Get all tags for media item
    virtual std::vector<std::string> getMediaTags(const std::string& mediaId) const = 0;
    
    /// Get all available tags in library
    virtual std::vector<std::string> getAllTags() const = 0;
    
    /// Get tag usage count
    virtual std::unordered_map<std::string, int32_t> getTagUsageCounts() const = 0;
    
    /// Rename tag globally
    virtual AsyncResult<VoidResult> renameTag(const std::string& oldTag, const std::string& newTag) = 0;
    
    /// Delete tag globally
    virtual AsyncResult<VoidResult> deleteTag(const std::string& tag) = 0;
    
    // ========================================================================
    // Collections and Playlists
    // ========================================================================
    
    /// Create collection
    virtual AsyncResult<VoidResult> createCollection(
        const std::string& collectionName,
        const std::string& description = ""
    ) = 0;
    
    /// Delete collection
    virtual AsyncResult<VoidResult> deleteCollection(const std::string& collectionName) = 0;
    
    /// Add media to collection
    virtual AsyncResult<VoidResult> addToCollection(
        const std::string& collectionName,
        const std::string& mediaId
    ) = 0;
    
    /// Remove media from collection
    virtual AsyncResult<VoidResult> removeFromCollection(
        const std::string& collectionName,
        const std::string& mediaId
    ) = 0;
    
    /// Get collection contents
    virtual std::vector<MediaInfo> getCollectionContents(const std::string& collectionName) const = 0;
    
    /// Get all collections
    virtual std::vector<std::string> getAllCollections() const = 0;
    
    /// Get collections for media item
    virtual std::vector<std::string> getMediaCollections(const std::string& mediaId) const = 0;
    
    // ========================================================================
    // Favorites and Ratings
    // ========================================================================
    
    /// Set media as favorite
    virtual AsyncResult<VoidResult> setFavorite(const std::string& mediaId, bool favorite = true) = 0;
    
    /// Set media rating
    virtual AsyncResult<VoidResult> setRating(const std::string& mediaId, float rating) = 0;  // 0.0-5.0
    
    /// Get favorite media
    virtual std::vector<MediaInfo> getFavoriteMedia() const = 0;
    
    /// Get highly rated media
    virtual std::vector<MediaInfo> getHighlyRatedMedia(float minRating = 4.0f) const = 0;
    
    /// Increment play count
    virtual AsyncResult<VoidResult> incrementPlayCount(const std::string& mediaId) = 0;
    
    /// Get most played media
    virtual std::vector<MediaInfo> getMostPlayedMedia(int32_t limit = 50) const = 0;
    
    // ========================================================================
    // Preview and Waveform Generation
    // ========================================================================
    
    /// Generate waveform data for media
    virtual AsyncResult<VoidResult> generateWaveform(
        const std::string& mediaId,
        int32_t width = 1024,
        int32_t height = 256
    ) = 0;
    
    /// Get waveform data
    virtual std::string getWaveformData(const std::string& mediaId) const = 0;
    
    /// Generate audio preview (short snippet)
    virtual AsyncResult<Result<std::string>> generatePreview(
        const std::string& mediaId,
        double startSeconds = 0.0,
        double durationSeconds = 10.0
    ) = 0;
    
    /// Generate spectrum analysis
    virtual AsyncResult<VoidResult> generateSpectrum(const std::string& mediaId) = 0;
    
    /// Get spectrum data
    virtual std::string getSpectrumData(const std::string& mediaId) const = 0;
    
    // ========================================================================
    // Audio Analysis
    // ========================================================================
    
    /// Analyze media file (BPM, key, etc.)
    virtual AsyncResult<VoidResult> analyzeMedia(const std::string& mediaId) = 0;
    
    /// Bulk analyze media files
    virtual AsyncResult<VoidResult> bulkAnalyzeMedia(
        const std::vector<std::string>& mediaIds,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Detect BPM
    virtual AsyncResult<Result<float>> detectBPM(const std::string& mediaId) = 0;
    
    /// Detect musical key
    virtual AsyncResult<Result<std::string>> detectKey(const std::string& mediaId) = 0;
    
    /// Detect beat markers
    virtual AsyncResult<Result<std::vector<float>>> detectBeats(const std::string& mediaId) = 0;
    
    /// Get analysis progress
    struct AnalysisProgress {
        int32_t totalItems = 0;
        int32_t analyzedItems = 0;
        int32_t failedItems = 0;
        std::string currentItem;
        bool isComplete = false;
    };
    
    virtual AnalysisProgress getAnalysisProgress() const = 0;
    
    /// Cancel ongoing analysis
    virtual VoidResult cancelAnalysis() = 0;
    
    // ========================================================================
    // Import and Export
    // ========================================================================
    
    /// Import media from other DAW projects
    virtual AsyncResult<Result<int32_t>> importFromProject(
        const std::string& projectPath,
        bool copyFiles = false,
        ProgressCallback progress = nullptr
    ) = 0;
    
    /// Export library database
    virtual AsyncResult<VoidResult> exportLibrary(
        const std::string& exportPath,
        bool includeFiles = false
    ) = 0;
    
    /// Import library database
    virtual AsyncResult<VoidResult> importLibrary(
        const std::string& importPath,
        bool merge = true
    ) = 0;
    
    /// Export collection as playlist
    virtual AsyncResult<VoidResult> exportCollectionAsPlaylist(
        const std::string& collectionName,
        const std::string& playlistPath
    ) = 0;
    
    /// Import playlist file
    virtual AsyncResult<Result<std::string>> importPlaylist(
        const std::string& playlistPath,
        const std::string& collectionName = ""
    ) = 0;
    
    // ========================================================================
    // Watch Folders and Auto-Import
    // ========================================================================
    
    /// Add watch folder
    virtual AsyncResult<VoidResult> addWatchFolder(
        const std::string& directoryPath,
        bool recursive = true
    ) = 0;
    
    /// Remove watch folder
    virtual AsyncResult<VoidResult> removeWatchFolder(const std::string& directoryPath) = 0;
    
    /// Get all watch folders
    virtual std::vector<std::string> getWatchFolders() const = 0;
    
    /// Enable/disable auto-import from watch folders
    virtual VoidResult setAutoImportEnabled(bool enabled) = 0;
    
    /// Check if auto-import is enabled
    virtual bool isAutoImportEnabled() const = 0;
    
    // ========================================================================
    // Database Management
    // ========================================================================
    
    /// Optimize library database
    virtual AsyncResult<VoidResult> optimizeDatabase() = 0;
    
    /// Rebuild library database
    virtual AsyncResult<VoidResult> rebuildDatabase(ProgressCallback progress = nullptr) = 0;
    
    /// Verify media file integrity
    virtual AsyncResult<Result<std::vector<std::string>>> verifyIntegrity() = 0;
    
    /// Clean up missing files
    virtual AsyncResult<Result<int32_t>> cleanupMissingFiles() = 0;
    
    /// Get database statistics
    struct DatabaseStats {
        int32_t totalMedia = 0;
        int32_t audioFiles = 0;
        int32_t midiFiles = 0;
        int32_t missingFiles = 0;
        int32_t totalTags = 0;
        int32_t totalCollections = 0;
        size_t databaseSize = 0;  // bytes
        std::chrono::system_clock::time_point lastOptimized;
        std::chrono::system_clock::time_point lastScan;
    };
    
    virtual DatabaseStats getDatabaseStats() const = 0;
    
    // ========================================================================
    // Format Support
    // ========================================================================
    
    /// Check if file format is supported
    virtual bool isFormatSupported(const std::string& fileExtension) const = 0;
    
    /// Get supported audio formats
    virtual std::vector<std::string> getSupportedAudioFormats() const = 0;
    
    /// Get supported MIDI formats
    virtual std::vector<std::string> getSupportedMIDIFormats() const = 0;
    
    /// Get all supported formats
    virtual std::vector<std::string> getAllSupportedFormats() const = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class LibraryEvent {
        ScanStarted,
        ScanProgress,
        ScanCompleted,
        MediaAdded,
        MediaRemoved,
        MediaUpdated,
        TagAdded,
        TagRemoved,
        CollectionCreated,
        CollectionDeleted,
        CollectionModified,
        AnalysisStarted,
        AnalysisCompleted,
        WatchFolderChanged
    };
    
    using LibraryEventCallback = std::function<void(LibraryEvent event, const std::string& details, const std::optional<std::string>& mediaId)>;
    
    /// Subscribe to library events
    virtual void addEventListener(LibraryEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(LibraryEventCallback callback) = 0;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Set media library cache directory
    virtual VoidResult setCacheDirectory(const std::string& directory) = 0;
    
    /// Get cache directory
    virtual std::string getCacheDirectory() const = 0;
    
    /// Clear cache
    virtual VoidResult clearCache() = 0;
    
    /// Set maximum cache size
    virtual VoidResult setMaxCacheSize(size_t sizeBytes) = 0;
    
    /// Enable/disable background processing
    virtual VoidResult setBackgroundProcessingEnabled(bool enabled) = 0;
    
    /// Check if background processing is enabled
    virtual bool isBackgroundProcessingEnabled() const = 0;
    
    /// Set analysis quality level
    enum class AnalysisQuality {
        Fast,       // Quick analysis, less accurate
        Balanced,   // Good balance of speed and accuracy
        High,       // Slower but more accurate
        Maximum     // Slowest but most accurate
    };
    
    virtual VoidResult setAnalysisQuality(AnalysisQuality quality) = 0;
    
    /// Get analysis quality level
    virtual AnalysisQuality getAnalysisQuality() const = 0;
};

} // namespace mixmind::core