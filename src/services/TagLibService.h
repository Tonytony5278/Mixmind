#pragma once

#include "IOSSService.h"
#include "../core/types.h"
#include "../core/result.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace mixmind::services {

// ============================================================================
// TagLib Service - Audio metadata reading and writing using TagLib
// ============================================================================

class TagLibService : public IMetadataService {
public:
    TagLibService();
    ~TagLibService() override;
    
    // Non-copyable, movable
    TagLibService(const TagLibService&) = delete;
    TagLibService& operator=(const TagLibService&) = delete;
    TagLibService(TagLibService&&) = default;
    TagLibService& operator=(TagLibService&&) = default;
    
    // ========================================================================
    // IOSSService Implementation
    // ========================================================================
    
    core::AsyncResult<core::VoidResult> initialize() override;
    core::AsyncResult<core::VoidResult> shutdown() override;
    bool isInitialized() const override;
    std::string getServiceName() const override;
    std::string getServiceVersion() const override;
    ServiceInfo getServiceInfo() const override;
    core::VoidResult configure(const std::unordered_map<std::string, std::string>& config) override;
    std::optional<std::string> getConfigValue(const std::string& key) const override;
    core::VoidResult resetConfiguration() override;
    bool isHealthy() const override;
    std::string getLastError() const override;
    core::AsyncResult<core::VoidResult> runSelfTest() override;
    PerformanceMetrics getPerformanceMetrics() const override;
    void resetPerformanceMetrics() override;
    
    // ========================================================================
    // IMetadataService Implementation
    // ========================================================================
    
    core::AsyncResult<core::Result<AudioMetadata>> readMetadata(const std::string& filePath) override;
    core::AsyncResult<core::VoidResult> writeMetadata(const std::string& filePath, const AudioMetadata& metadata) override;
    bool isFormatSupported(const std::string& fileExtension) const override;
    std::vector<std::string> getSupportedFormats() const override;
    core::AsyncResult<core::VoidResult> clearMetadata(const std::string& filePath) override;
    
    // ========================================================================
    // Advanced Metadata Operations
    // ========================================================================
    
    /// Extended metadata structure with all TagLib properties
    struct ExtendedMetadata : public AudioMetadata {
        // ID3v2 specific
        std::string albumArtist;
        std::string composer;
        std::string conductor;
        std::string copyright;
        std::string encodedBy;
        std::string grouping;
        std::string lyrics;
        std::string originalArtist;
        std::string originalDate;
        std::string publisher;
        std::string subtitle;
        std::string website;
        std::string isrc;          // International Standard Recording Code
        std::string musicBrainzID;
        
        // Audio properties
        int32_t bitDepth = 0;
        std::string codec;
        bool isLossless = false;
        bool isVariableBitRate = false;
        
        // Replay Gain
        float replayGainTrackGain = 0.0f;
        float replayGainTrackPeak = 0.0f;
        float replayGainAlbumGain = 0.0f;
        float replayGainAlbumPeak = 0.0f;
        
        // Additional technical info
        std::string encoder;
        std::string encoderSettings;
        std::chrono::system_clock::time_point dateTagged;
        std::chrono::system_clock::time_point dateEncoded;
        
        // Cover art
        struct CoverArt {
            std::vector<uint8_t> data;
            std::string mimeType;
            std::string description;
            enum Type {
                Other = 0,
                FileIcon = 1,
                OtherFileIcon = 2,
                FrontCover = 3,
                BackCover = 4,
                LeafletPage = 5,
                Media = 6,
                LeadArtist = 7,
                Artist = 8,
                Conductor = 9,
                Band = 10,
                Composer = 11,
                Lyricist = 12,
                RecordingLocation = 13,
                DuringRecording = 14,
                DuringPerformance = 15,
                MovieScreenCapture = 16,
                ColouredFish = 17,
                Illustration = 18,
                BandLogo = 19,
                PublisherLogo = 20
            } type = FrontCover;
        };
        
        std::vector<CoverArt> coverArt;
    };
    
    /// Read extended metadata with all available properties
    core::AsyncResult<core::Result<ExtendedMetadata>> readExtendedMetadata(const std::string& filePath);
    
    /// Write extended metadata
    core::AsyncResult<core::VoidResult> writeExtendedMetadata(const std::string& filePath, const ExtendedMetadata& metadata);
    
    /// Get all available metadata properties for a file
    core::AsyncResult<core::Result<TagLib::PropertyMap>> getAllProperties(const std::string& filePath);
    
    /// Set custom property
    core::AsyncResult<core::VoidResult> setCustomProperty(
        const std::string& filePath, 
        const std::string& key, 
        const std::string& value
    );
    
    /// Remove custom property
    core::AsyncResult<core::VoidResult> removeCustomProperty(
        const std::string& filePath, 
        const std::string& key
    );
    
    // ========================================================================
    // Cover Art Management
    // ========================================================================
    
    /// Extract cover art from file
    core::AsyncResult<core::Result<std::vector<ExtendedMetadata::CoverArt>>> extractCoverArt(const std::string& filePath);
    
    /// Add cover art to file
    core::AsyncResult<core::VoidResult> addCoverArt(
        const std::string& filePath,
        const ExtendedMetadata::CoverArt& coverArt
    );
    
    /// Remove all cover art from file
    core::AsyncResult<core::VoidResult> removeCoverArt(const std::string& filePath);
    
    /// Replace cover art in file
    core::AsyncResult<core::VoidResult> replaceCoverArt(
        const std::string& filePath,
        const std::vector<ExtendedMetadata::CoverArt>& coverArt
    );
    
    /// Load cover art from image file
    core::Result<ExtendedMetadata::CoverArt> loadCoverArtFromFile(
        const std::string& imagePath,
        ExtendedMetadata::CoverArt::Type type = ExtendedMetadata::CoverArt::FrontCover,
        const std::string& description = ""
    );
    
    /// Save cover art to image file
    core::VoidResult saveCoverArtToFile(
        const ExtendedMetadata::CoverArt& coverArt,
        const std::string& imagePath
    );
    
    // ========================================================================
    // Batch Operations
    // ========================================================================
    
    /// Read metadata from multiple files
    core::AsyncResult<core::Result<std::vector<std::pair<std::string, AudioMetadata>>>> readMetadataBatch(
        const std::vector<std::string>& filePaths,
        core::ProgressCallback progress = nullptr
    );
    
    /// Write metadata to multiple files
    core::AsyncResult<core::VoidResult> writeMetadataBatch(
        const std::vector<std::pair<std::string, AudioMetadata>>& fileMetadata,
        core::ProgressCallback progress = nullptr
    );
    
    /// Update specific field across multiple files
    core::AsyncResult<core::VoidResult> updateFieldBatch(
        const std::vector<std::string>& filePaths,
        const std::string& fieldName,
        const std::string& value,
        core::ProgressCallback progress = nullptr
    );
    
    /// Clear metadata from multiple files
    core::AsyncResult<core::VoidResult> clearMetadataBatch(
        const std::vector<std::string>& filePaths,
        core::ProgressCallback progress = nullptr
    );
    
    // ========================================================================
    // Metadata Validation and Cleanup
    // ========================================================================
    
    /// Validation result
    struct ValidationResult {
        bool isValid = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        std::unordered_map<std::string, std::string> suggestions; // field -> suggested value
    };
    
    /// Validate metadata consistency and standards compliance
    ValidationResult validateMetadata(const ExtendedMetadata& metadata) const;
    
    /// Clean up metadata (remove empty fields, fix encoding issues)
    ExtendedMetadata cleanupMetadata(const ExtendedMetadata& metadata) const;
    
    /// Normalize metadata (standardize formats, case, etc.)
    ExtendedMetadata normalizeMetadata(const ExtendedMetadata& metadata) const;
    
    /// Check for duplicate metadata across files
    core::AsyncResult<core::Result<std::vector<std::vector<std::string>>>> findDuplicatesByMetadata(
        const std::vector<std::string>& filePaths,
        const std::vector<std::string>& compareFields = {"title", "artist", "duration"}
    );
    
    // ========================================================================
    // Format-Specific Operations
    // ========================================================================
    
    /// ID3v2 tag version management
    enum class ID3Version {
        ID3v1,
        ID3v2_3,
        ID3v2_4
    };
    
    /// Set preferred ID3 version for writing
    core::VoidResult setPreferredID3Version(ID3Version version);
    
    /// Get preferred ID3 version
    ID3Version getPreferredID3Version() const;
    
    /// Convert between ID3 versions
    core::AsyncResult<core::VoidResult> convertID3Version(
        const std::string& filePath,
        ID3Version targetVersion
    );
    
    /// Remove ID3v1 tags (keep only ID3v2)
    core::AsyncResult<core::VoidResult> removeID3v1Tags(const std::string& filePath);
    
    /// FLAC-specific: Get/set Vorbis comments
    core::AsyncResult<core::Result<std::unordered_map<std::string, std::string>>> getFLACVorbisComments(
        const std::string& filePath
    );
    
    core::AsyncResult<core::VoidResult> setFLACVorbisComments(
        const std::string& filePath,
        const std::unordered_map<std::string, std::string>& comments
    );
    
    // ========================================================================
    // Encoding and Character Set Management
    // ========================================================================
    
    /// Text encoding options
    enum class TextEncoding {
        Latin1,
        UTF8,
        UTF16,
        UTF16BE,
        UTF16LE
    };
    
    /// Set text encoding for writing metadata
    core::VoidResult setTextEncoding(TextEncoding encoding);
    
    /// Get current text encoding
    TextEncoding getTextEncoding() const;
    
    /// Fix encoding issues in metadata
    core::AsyncResult<core::VoidResult> fixTextEncoding(const std::string& filePath);
    
    /// Detect text encoding in existing metadata
    core::Result<TextEncoding> detectTextEncoding(const std::string& filePath);
    
    // ========================================================================
    // Metadata Import/Export
    // ========================================================================
    
    /// Export metadata to JSON
    core::VoidResult exportMetadataToJSON(
        const ExtendedMetadata& metadata,
        const std::string& jsonPath
    );
    
    /// Import metadata from JSON
    core::AsyncResult<core::Result<ExtendedMetadata>> importMetadataFromJSON(
        const std::string& jsonPath
    );
    
    /// Export metadata to CSV
    core::VoidResult exportMetadataToCSV(
        const std::vector<std::pair<std::string, ExtendedMetadata>>& fileMetadata,
        const std::string& csvPath
    );
    
    /// Import metadata from CSV
    core::AsyncResult<core::Result<std::vector<std::pair<std::string, ExtendedMetadata>>>> importMetadataFromCSV(
        const std::string& csvPath
    );
    
    /// Export to MusicBrainz Picard format
    core::VoidResult exportToPicardFormat(
        const std::vector<std::pair<std::string, ExtendedMetadata>>& fileMetadata,
        const std::string& outputPath
    );
    
    // ========================================================================
    // Online Database Integration
    // ========================================================================
    
    /// MusicBrainz lookup data
    struct MusicBrainzData {
        std::string recordingId;
        std::string releaseId;
        std::string artistId;
        std::string albumId;
        float acoustidScore = 0.0f;
        bool verified = false;
    };
    
    /// Lookup metadata in online databases (placeholder for future implementation)
    core::AsyncResult<core::Result<ExtendedMetadata>> lookupOnlineMetadata(
        const std::string& filePath,
        const std::string& fingerprint = ""
    );
    
    /// Generate audio fingerprint for database lookup
    core::AsyncResult<core::Result<std::string>> generateAudioFingerprint(
        const std::string& filePath
    );
    
    // ========================================================================
    // Statistics and Analysis
    // ========================================================================
    
    struct MetadataStatistics {
        int32_t totalFiles = 0;
        int32_t filesWithMetadata = 0;
        int32_t filesWithCoverArt = 0;
        std::unordered_map<std::string, int32_t> formatCounts;
        std::unordered_map<std::string, int32_t> encoderCounts;
        std::unordered_map<int32_t, int32_t> yearCounts;
        std::unordered_map<std::string, int32_t> genreCounts;
        float averageBitRate = 0.0f;
        double totalDuration = 0.0;
    };
    
    /// Analyze metadata statistics across multiple files
    core::AsyncResult<core::Result<MetadataStatistics>> analyzeMetadataStatistics(
        const std::vector<std::string>& filePaths,
        core::ProgressCallback progress = nullptr
    );
    
    /// Find files missing specific metadata fields
    core::AsyncResult<core::Result<std::vector<std::string>>> findFilesMissingMetadata(
        const std::vector<std::string>& filePaths,
        const std::vector<std::string>& requiredFields
    );
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Create TagLib FileRef for file
    std::unique_ptr<TagLib::FileRef> createFileRef(const std::string& filePath);
    
    /// Convert TagLib string to std::string
    std::string tagLibStringToStdString(const TagLib::String& tagString) const;
    
    /// Convert std::string to TagLib string
    TagLib::String stdStringToTagLibString(const std::string& stdString) const;
    
    /// Map TagLib properties to our metadata structure
    AudioMetadata mapTagLibToMetadata(TagLib::Tag* tag, TagLib::AudioProperties* properties) const;
    
    /// Map our metadata to TagLib properties
    void mapMetadataToTagLib(const AudioMetadata& metadata, TagLib::Tag* tag) const;
    
    /// Extract extended properties from TagLib PropertyMap
    void extractExtendedProperties(
        const TagLib::PropertyMap& propertyMap, 
        ExtendedMetadata& metadata
    ) const;
    
    /// Update performance metrics
    void updatePerformanceMetrics(double processingTime, bool success);
    
    /// Validate file path and format
    core::VoidResult validateFile(const std::string& filePath) const;
    
    /// Get mime type from file extension
    std::string getMimeTypeFromExtension(const std::string& extension) const;
    
private:
    // Service state
    std::atomic<bool> isInitialized_{false};
    
    // Configuration
    std::unordered_map<std::string, std::string> config_;
    mutable std::mutex configMutex_;
    
    // Format preferences
    ID3Version preferredID3Version_ = ID3Version::ID3v2_4;
    TextEncoding textEncoding_ = TextEncoding::UTF8;
    
    // Supported formats
    std::vector<std::string> supportedFormats_;
    std::unordered_map<std::string, std::string> mimeTypeMap_;
    
    // Performance tracking
    mutable PerformanceMetrics performanceMetrics_;
    mutable std::mutex metricsMutex_;
    
    // Error tracking
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;
};

} // namespace mixmind::services