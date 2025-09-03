#pragma once

#include "types.h"
#include "result.h"
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

namespace mixmind::core {

// Forward declaration
class IPluginInstance;

// ============================================================================
// Plugin Host Interface - Plugin scanning, loading, and management
// ============================================================================

class IPluginHost {
public:
    virtual ~IPluginHost() = default;
    
    // ========================================================================
    // Plugin Scanning and Discovery
    // ========================================================================
    
    /// Scan for plugins in default directories
    virtual AsyncResult<Result<int32_t>> scanPlugins(ProgressCallback progress = nullptr) = 0;
    
    /// Scan for plugins in specific directory
    virtual AsyncResult<Result<int32_t>> scanDirectory(const std::string& directoryPath, 
                                                       ProgressCallback progress = nullptr) = 0;
    
    /// Scan specific plugin file
    virtual AsyncResult<Result<bool>> scanPluginFile(const std::string& filePath) = 0;
    
    /// Get scan progress information
    struct ScanProgress {
        int32_t totalPlugins = 0;
        int32_t scannedPlugins = 0;
        int32_t validPlugins = 0;
        int32_t failedPlugins = 0;
        std::string currentPlugin;
        std::vector<std::string> failedPluginPaths;
        bool isComplete = false;
    };
    
    /// Get current scan progress
    virtual ScanProgress getScanProgress() const = 0;
    
    /// Cancel ongoing scan
    virtual VoidResult cancelScan() = 0;
    
    /// Check if scan is in progress
    virtual bool isScanInProgress() const = 0;
    
    /// Force rescan of specific plugin
    virtual AsyncResult<VoidResult> rescanPlugin(const PluginID& pluginId) = 0;
    
    /// Clear plugin database and rescan everything
    virtual AsyncResult<Result<int32_t>> fullRescan(ProgressCallback progress = nullptr) = 0;
    
    // ========================================================================
    // Plugin Database and Registry
    // ========================================================================
    
    /// Get all discovered plugins
    virtual std::vector<PluginInfo> getAllPlugins() const = 0;
    
    /// Get plugins by type
    virtual std::vector<PluginInfo> getPluginsByType(PluginType type) const = 0;
    
    /// Get plugins by category
    virtual std::vector<PluginInfo> getPluginsByCategory(PluginCategory category) const = 0;
    
    /// Find plugin by ID
    virtual std::optional<PluginInfo> getPluginInfo(const PluginID& pluginId) const = 0;
    
    /// Find plugins by name (fuzzy search)
    virtual std::vector<PluginInfo> findPluginsByName(const std::string& name) const = 0;
    
    /// Find plugins by manufacturer
    virtual std::vector<PluginInfo> getPluginsByManufacturer(const std::string& manufacturer) const = 0;
    
    /// Get plugin count
    virtual int32_t getPluginCount() const = 0;
    
    /// Get plugin count by type
    virtual std::unordered_map<PluginType, int32_t> getPluginCountsByType() const = 0;
    
    /// Check if plugin exists and is valid
    virtual bool isPluginValid(const PluginID& pluginId) const = 0;
    
    /// Get plugin file path
    virtual std::string getPluginFilePath(const PluginID& pluginId) const = 0;
    
    // ========================================================================
    // Plugin Loading and Instantiation
    // ========================================================================
    
    /// Load plugin instance
    virtual AsyncResult<Result<std::shared_ptr<IPluginInstance>>> loadPlugin(const PluginID& pluginId) = 0;
    
    /// Load plugin with specific configuration
    struct PluginLoadConfig {
        PluginID pluginId;
        SampleRate sampleRate = 44100;
        BufferSize maxBufferSize = 1024;
        int32_t numInputs = 2;
        int32_t numOutputs = 2;
        bool loadEditor = true;
        std::string presetPath;
        std::unordered_map<ParamID, float> initialParameters;
    };
    
    /// Load plugin with configuration
    virtual AsyncResult<Result<std::shared_ptr<IPluginInstance>>> loadPlugin(const PluginLoadConfig& config) = 0;
    
    /// Unload plugin instance
    virtual AsyncResult<VoidResult> unloadPlugin(PluginInstanceID instanceId) = 0;
    
    /// Get loaded plugin instance
    virtual std::shared_ptr<IPluginInstance> getPluginInstance(PluginInstanceID instanceId) = 0;
    
    /// Get all loaded plugin instances
    virtual std::vector<std::shared_ptr<IPluginInstance>> getAllLoadedPlugins() = 0;
    
    /// Get number of loaded plugins
    virtual int32_t getLoadedPluginCount() const = 0;
    
    // ========================================================================
    // Plugin Directories and Paths
    // ========================================================================
    
    /// Add plugin scan directory
    virtual VoidResult addScanDirectory(const std::string& directoryPath) = 0;
    
    /// Remove plugin scan directory
    virtual VoidResult removeScanDirectory(const std::string& directoryPath) = 0;
    
    /// Get all scan directories
    virtual std::vector<std::string> getScanDirectories() const = 0;
    
    /// Get default scan directories for each plugin type
    virtual std::unordered_map<PluginType, std::vector<std::string>> getDefaultScanDirectories() const = 0;
    
    /// Reset scan directories to defaults
    virtual VoidResult resetScanDirectoriesToDefaults() = 0;
    
    // ========================================================================
    // Plugin Blacklisting and Management
    // ========================================================================
    
    /// Blacklist plugin (prevent loading)
    virtual VoidResult blacklistPlugin(const PluginID& pluginId, const std::string& reason = "") = 0;
    
    /// Remove plugin from blacklist
    virtual VoidResult removeFromBlacklist(const PluginID& pluginId) = 0;
    
    /// Check if plugin is blacklisted
    virtual bool isPluginBlacklisted(const PluginID& pluginId) const = 0;
    
    /// Get blacklisted plugins
    virtual std::vector<PluginID> getBlacklistedPlugins() const = 0;
    
    /// Get blacklist reason
    virtual std::string getBlacklistReason(const PluginID& pluginId) const = 0;
    
    /// Clear entire blacklist
    virtual VoidResult clearBlacklist() = 0;
    
    // ========================================================================
    // Plugin Favorites and Organization
    // ========================================================================
    
    /// Add plugin to favorites
    virtual VoidResult addToFavorites(const PluginID& pluginId) = 0;
    
    /// Remove plugin from favorites
    virtual VoidResult removeFromFavorites(const PluginID& pluginId) = 0;
    
    /// Check if plugin is favorited
    virtual bool isPluginFavorited(const PluginID& pluginId) const = 0;
    
    /// Get favorite plugins
    virtual std::vector<PluginInfo> getFavoritePlugins() const = 0;
    
    /// Create custom plugin collection
    virtual VoidResult createCollection(const std::string& name, 
                                       const std::vector<PluginID>& plugins = {}) = 0;
    
    /// Add plugin to collection
    virtual VoidResult addToCollection(const std::string& collectionName, const PluginID& pluginId) = 0;
    
    /// Remove plugin from collection
    virtual VoidResult removeFromCollection(const std::string& collectionName, const PluginID& pluginId) = 0;
    
    /// Get plugins in collection
    virtual std::vector<PluginInfo> getCollection(const std::string& collectionName) const = 0;
    
    /// Get all collection names
    virtual std::vector<std::string> getCollectionNames() const = 0;
    
    /// Delete collection
    virtual VoidResult deleteCollection(const std::string& collectionName) = 0;
    
    // ========================================================================
    // Plugin Presets and State Management
    // ========================================================================
    
    /// Get available presets for plugin
    virtual std::vector<std::string> getPluginPresets(const PluginID& pluginId) const = 0;
    
    /// Save plugin state as preset
    virtual AsyncResult<VoidResult> savePreset(PluginInstanceID instanceId, 
                                               const std::string& presetName,
                                               const std::string& description = "") = 0;
    
    /// Load preset into plugin
    virtual AsyncResult<VoidResult> loadPreset(PluginInstanceID instanceId, 
                                              const std::string& presetName) = 0;
    
    /// Delete preset
    virtual VoidResult deletePreset(const PluginID& pluginId, const std::string& presetName) = 0;
    
    /// Import preset from file
    virtual AsyncResult<VoidResult> importPreset(const PluginID& pluginId, 
                                                 const std::string& filePath) = 0;
    
    /// Export preset to file
    virtual AsyncResult<VoidResult> exportPreset(const PluginID& pluginId,
                                                 const std::string& presetName,
                                                 const std::string& filePath) = 0;
    
    // ========================================================================
    // Plugin Validation and Testing
    // ========================================================================
    
    /// Validate plugin (test loading without creating instance)
    virtual AsyncResult<Result<bool>> validatePlugin(const PluginID& pluginId) = 0;
    
    /// Run comprehensive plugin test
    struct PluginTestResult {
        bool canLoad = false;
        bool canProcessAudio = false;
        bool hasValidEditor = false;
        bool respondsToParameters = false;
        bool canSaveState = false;
        std::vector<std::string> issues;
        double loadTime = 0.0;  // seconds
        size_t memoryUsage = 0; // bytes
    };
    
    /// Test plugin thoroughly
    virtual AsyncResult<Result<PluginTestResult>> testPlugin(const PluginID& pluginId) = 0;
    
    /// Get plugin compatibility score (0.0 to 1.0)
    virtual float getPluginCompatibilityScore(const PluginID& pluginId) const = 0;
    
    // ========================================================================
    // Plugin Format Support
    // ========================================================================
    
    /// Check if plugin format is supported
    virtual bool isFormatSupported(PluginType type) const = 0;
    
    /// Get supported plugin formats
    virtual std::vector<PluginType> getSupportedFormats() const = 0;
    
    /// Get format-specific information
    struct FormatInfo {
        PluginType type;
        std::string name;
        std::string version;
        std::vector<std::string> fileExtensions;
        std::vector<std::string> defaultPaths;
        bool supportsEditors = true;
        bool supportsPresets = true;
        bool supportsMIDI = true;
        bool supportsSidechain = false;
    };
    
    /// Get format information
    virtual FormatInfo getFormatInfo(PluginType type) const = 0;
    
    // ========================================================================
    // Built-in Plugins
    // ========================================================================
    
    /// Register built-in plugin
    virtual VoidResult registerBuiltInPlugin(const PluginInfo& info,
                                             std::function<std::shared_ptr<IPluginInstance>()> factory) = 0;
    
    /// Unregister built-in plugin
    virtual VoidResult unregisterBuiltInPlugin(const PluginID& pluginId) = 0;
    
    /// Get built-in plugins
    virtual std::vector<PluginInfo> getBuiltInPlugins() const = 0;
    
    /// Check if plugin is built-in
    virtual bool isBuiltInPlugin(const PluginID& pluginId) const = 0;
    
    // ========================================================================
    // Plugin Performance and Resource Management
    // ========================================================================
    
    /// Get plugin CPU usage
    virtual float getPluginCPUUsage(PluginInstanceID instanceId) const = 0;
    
    /// Get plugin memory usage
    virtual size_t getPluginMemoryUsage(PluginInstanceID instanceId) const = 0;
    
    /// Get total plugin system CPU usage
    virtual float getTotalPluginCPUUsage() const = 0;
    
    /// Get total plugin system memory usage
    virtual size_t getTotalPluginMemoryUsage() const = 0;
    
    /// Set plugin processing priority
    enum class ProcessingPriority {
        Low,
        Normal,
        High,
        Realtime
    };
    
    virtual VoidResult setPluginPriority(PluginInstanceID instanceId, ProcessingPriority priority) = 0;
    
    /// Get plugin processing priority
    virtual ProcessingPriority getPluginPriority(PluginInstanceID instanceId) const = 0;
    
    /// Enable/disable plugin multicore processing
    virtual VoidResult setMulticoreProcessingEnabled(bool enabled) = 0;
    
    /// Check if multicore processing is enabled
    virtual bool isMulticoreProcessingEnabled() const = 0;
    
    /// Set maximum number of processing threads
    virtual VoidResult setMaxProcessingThreads(int32_t maxThreads) = 0;
    
    /// Get maximum number of processing threads
    virtual int32_t getMaxProcessingThreads() const = 0;
    
    // ========================================================================
    // Plugin Sandboxing and Security
    // ========================================================================
    
    /// Enable/disable plugin sandboxing
    virtual VoidResult setSandboxingEnabled(bool enabled) = 0;
    
    /// Check if sandboxing is enabled
    virtual bool isSandboxingEnabled() const = 0;
    
    /// Set plugin crash recovery mode
    enum class CrashRecoveryMode {
        None,           // No recovery
        Bypass,         // Bypass crashed plugin
        Restart,        // Restart crashed plugin
        Substitute      // Replace with safe alternative
    };
    
    virtual VoidResult setCrashRecoveryMode(CrashRecoveryMode mode) = 0;
    
    /// Get crash recovery mode
    virtual CrashRecoveryMode getCrashRecoveryMode() const = 0;
    
    /// Get plugin crash count
    virtual int32_t getPluginCrashCount(const PluginID& pluginId) const = 0;
    
    /// Reset plugin crash count
    virtual VoidResult resetPluginCrashCount(const PluginID& pluginId) = 0;
    
    // ========================================================================
    // Plugin Bridge and Compatibility
    // ========================================================================
    
    /// Enable/disable plugin bridging (32-bit plugins in 64-bit host)
    virtual VoidResult setBridgingEnabled(bool enabled) = 0;
    
    /// Check if bridging is enabled
    virtual bool isBridgingEnabled() const = 0;
    
    /// Set bridge timeout
    virtual VoidResult setBridgeTimeout(int32_t timeoutMs) = 0;
    
    /// Get bridge timeout
    virtual int32_t getBridgeTimeout() const = 0;
    
    /// Check if plugin requires bridging
    virtual bool requiresBridging(const PluginID& pluginId) const = 0;
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class PluginHostEvent {
        ScanStarted,
        ScanProgress,
        ScanCompleted,
        PluginAdded,
        PluginRemoved,
        PluginLoaded,
        PluginUnloaded,
        PluginCrashed,
        PluginBlacklisted,
        PluginFavorited,
        CollectionChanged
    };
    
    using PluginHostEventCallback = std::function<void(PluginHostEvent event, 
                                                       const std::string& details,
                                                       const std::optional<PluginID>& pluginId)>;
    
    /// Subscribe to plugin host events
    virtual void addEventListener(PluginHostEventCallback callback) = 0;
    
    /// Remove event listener
    virtual void removeEventListener(PluginHostEventCallback callback) = 0;
    
    // ========================================================================
    // Database and Persistence
    // ========================================================================
    
    /// Export plugin database to file
    virtual AsyncResult<VoidResult> exportDatabase(const std::string& filePath) = 0;
    
    /// Import plugin database from file
    virtual AsyncResult<VoidResult> importDatabase(const std::string& filePath, bool merge = true) = 0;
    
    /// Clear plugin database
    virtual AsyncResult<VoidResult> clearDatabase() = 0;
    
    /// Get database statistics
    struct DatabaseStats {
        int32_t totalPlugins = 0;
        int32_t validPlugins = 0;
        int32_t blacklistedPlugins = 0;
        int32_t favoritePlugins = 0;
        int32_t collections = 0;
        int32_t presets = 0;
        size_t databaseSize = 0;  // bytes
        std::chrono::system_clock::time_point lastScanTime;
    };
    
    /// Get database statistics
    virtual DatabaseStats getDatabaseStats() const = 0;
    
    // ========================================================================
    // Advanced Features
    // ========================================================================
    
    /// Set plugin load timeout
    virtual VoidResult setPluginLoadTimeout(int32_t timeoutMs) = 0;
    
    /// Get plugin load timeout
    virtual int32_t getPluginLoadTimeout() const = 0;
    
    /// Enable/disable plugin editor scaling
    virtual VoidResult setEditorScalingEnabled(bool enabled) = 0;
    
    /// Check if editor scaling is enabled
    virtual bool isEditorScalingEnabled() const = 0;
    
    /// Set editor scale factor
    virtual VoidResult setEditorScaleFactor(float scaleFactor) = 0;
    
    /// Get editor scale factor
    virtual float getEditorScaleFactor() const = 0;
    
    /// Enable/disable plugin parameter automation
    virtual VoidResult setParameterAutomationEnabled(bool enabled) = 0;
    
    /// Check if parameter automation is enabled
    virtual bool isParameterAutomationEnabled() const = 0;
    
    /// Set audio thread priority
    virtual VoidResult setAudioThreadPriority(ProcessingPriority priority) = 0;
    
    /// Get audio thread priority
    virtual ProcessingPriority getAudioThreadPriority() const = 0;
};

} // namespace mixmind::core