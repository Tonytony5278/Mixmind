#pragma once

#include "../../core/types.h"
#include "../../core/result.h"
#include "../../core/async.h"
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>

namespace mixmind::adapters::tracktion {

// ============================================================================
// VST3 Plugin Information
// ============================================================================

struct VST3PluginInfo {
    std::string name;
    std::string vendor;
    std::string category;
    std::string version;
    std::string uid;  // Unique identifier
    std::string filePath;
    
    // Capabilities
    bool hasEditor = false;
    bool isSynth = false;
    bool isEffect = true;
    int numAudioInputs = 2;
    int numAudioOutputs = 2;
    int numMidiInputs = 1;
    int numMidiOutputs = 0;
    
    // Scan results
    bool isBlacklisted = false;
    bool scanSuccessful = true;
    std::string scanError;
    std::chrono::system_clock::time_point lastScanned;
    
    bool isValid() const {
        return scanSuccessful && !isBlacklisted && !uid.empty();
    }
};

// ============================================================================
// VST3 Plugin Scanner - Discovers and catalogs VST3 plugins
// ============================================================================

class TEVSTScanner {
public:
    TEVSTScanner();
    ~TEVSTScanner();
    
    // Non-copyable
    TEVSTScanner(const TEVSTScanner&) = delete;
    TEVSTScanner& operator=(const TEVSTScanner&) = delete;
    
    // ========================================================================
    // Scanning Operations
    // ========================================================================
    
    /// Scan all system VST3 directories
    core::AsyncResult<core::VoidResult> scanAllDirectories();
    
    /// Scan specific directory for VST3 plugins  
    core::AsyncResult<core::VoidResult> scanDirectory(const std::string& directoryPath);
    
    /// Scan single VST3 file
    core::AsyncResult<core::Result<VST3PluginInfo>> scanPluginFile(const std::string& filePath);
    
    /// Re-scan all previously discovered plugins
    core::AsyncResult<core::VoidResult> rescanAll();
    
    // ========================================================================
    // Plugin Database
    // ========================================================================
    
    /// Get all discovered plugins
    std::vector<VST3PluginInfo> getAllPlugins() const;
    
    /// Get plugins by category
    std::vector<VST3PluginInfo> getPluginsByCategory(const std::string& category) const;
    
    /// Find plugin by UID
    std::optional<VST3PluginInfo> findPluginByUID(const std::string& uid) const;
    
    /// Find plugin by name
    std::vector<VST3PluginInfo> findPluginsByName(const std::string& name) const;
    
    // ========================================================================
    // Blacklist Management
    // ========================================================================
    
    /// Add plugin to blacklist
    void blacklistPlugin(const std::string& uid, const std::string& reason = "");
    
    /// Remove plugin from blacklist
    void unblacklistPlugin(const std::string& uid);
    
    /// Check if plugin is blacklisted
    bool isPluginBlacklisted(const std::string& uid) const;
    
    /// Get blacklisted plugin UIDs
    std::vector<std::string> getBlacklistedPlugins() const;
    
    // ========================================================================
    // Cache Management
    // ========================================================================
    
    /// Save plugin database to cache file
    core::VoidResult saveCache(const std::string& cacheFilePath = "") const;
    
    /// Load plugin database from cache file
    core::VoidResult loadCache(const std::string& cacheFilePath = "");
    
    /// Clear all cached plugin information
    void clearCache();
    
    /// Get cache statistics
    struct CacheStats {
        int totalPlugins = 0;
        int validPlugins = 0;
        int blacklistedPlugins = 0;
        int failedScans = 0;
        std::chrono::system_clock::time_point lastScan;
    };
    
    CacheStats getCacheStats() const;
    
    // ========================================================================
    // Progress Reporting
    // ========================================================================
    
    /// Set progress callback for scan operations
    void setProgressCallback(std::function<void(const std::string&, float)> callback);
    
private:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Get default VST3 directories for the current platform
    std::vector<std::string> getDefaultVST3Directories() const;
    
    /// Recursively find all .vst3 files in directory
    std::vector<std::string> findVST3Files(const std::string& directoryPath) const;
    
    /// Actually scan a VST3 file and extract information
    core::Result<VST3PluginInfo> scanVST3File(const std::string& filePath) const;
    
    /// Extract plugin information from VST3 module
    core::Result<VST3PluginInfo> extractPluginInfo(
        const std::string& filePath,
        VST::Hosting::Module::Ptr module,
        const VST::Hosting::PluginFactory& factory
    ) const;
    
    /// Report progress to callback
    void reportProgress(const std::string& message, float progress);
    
    /// Thread-safe plugin database access
    mutable std::recursive_mutex databaseMutex_;
    
    /// Plugin database
    std::vector<VST3PluginInfo> pluginDatabase_;
    
    /// Blacklisted plugin UIDs with reasons
    std::unordered_map<std::string, std::string> blacklistedPlugins_;
    
    /// Progress callback
    std::function<void(const std::string&, float)> progressCallback_;
    
    /// Default cache file path
    std::string defaultCacheFile_;
    
    /// Scan statistics
    mutable CacheStats lastStats_;
};

// ============================================================================
// Global Scanner Instance
// ============================================================================

/// Get the global VST3 scanner instance
TEVSTScanner& getGlobalVSTScanner();

} // namespace mixmind::adapters::tracktion