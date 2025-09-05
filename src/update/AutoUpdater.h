#pragma once

#include "../core/result.h"
#include "../core/async.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>

namespace mixmind::update {

// ============================================================================
// PRODUCTION: Auto-Update System
// Handles checking for updates, downloading, and applying them seamlessly
// ============================================================================

class AutoUpdater {
public:
    struct Version {
        int major = 0;
        int minor = 0;
        int patch = 0;
        std::string prerelease;
        
        Version() = default;
        Version(int maj, int min, int pat, const std::string& pre = "")
            : major(maj), minor(min), patch(pat), prerelease(pre) {}
        
        explicit Version(const std::string& versionString);
        
        std::string toString() const;
        bool isNewerThan(const Version& other) const;
        bool isValid() const { return major >= 0 && minor >= 0 && patch >= 0; }
        
        bool operator>(const Version& other) const { return isNewerThan(other); }
        bool operator==(const Version& other) const;
        bool operator!=(const Version& other) const { return !(*this == other); }
    };
    
    struct UpdateInfo {
        Version version;
        std::string downloadUrl;
        std::string changelogUrl;
        std::string releaseNotes;
        size_t downloadSizeBytes = 0;
        std::string signature;
        std::string checksum;
        bool isCritical = false;
        bool requiresRestart = true;
        std::chrono::system_clock::time_point releaseDate;
        
        bool isValid() const {
            return version.isValid() && !downloadUrl.empty() && !signature.empty();
        }
    };
    
    enum class UpdateChannel {
        STABLE,     // Stable releases only
        BETA,       // Beta and stable releases  
        ALPHA       // All releases including alpha
    };
    
    enum class UpdateResult {
        SUCCESS,
        NO_UPDATE_AVAILABLE,
        DOWNLOAD_FAILED,
        VERIFICATION_FAILED,
        INSTALLATION_FAILED,
        NETWORK_ERROR,
        PERMISSION_DENIED,
        DISK_SPACE_INSUFFICIENT,
        USER_CANCELLED
    };
    
    enum class UpdateStatus {
        IDLE,
        CHECKING,
        DOWNLOADING,
        VERIFYING,
        INSTALLING,
        COMPLETED,
        FAILED
    };
    
    // Progress callback: (current bytes, total bytes, status message)
    using ProgressCallback = std::function<void(size_t, size_t, const std::string&)>;
    
    // Update availability callback: (update info)
    using UpdateAvailableCallback = std::function<void(const UpdateInfo&)>;
    
    // Update completion callback: (result, error message)
    using UpdateCompleteCallback = std::function<void(UpdateResult, const std::string&)>;
    
    AutoUpdater();
    ~AutoUpdater();
    
    // Configuration
    void setUpdateServerUrl(const std::string& url) { updateServerUrl_ = url; }
    void setUpdateChannel(UpdateChannel channel) { updateChannel_ = channel; }
    void setCurrentVersion(const Version& version) { currentVersion_ = version; }
    void setPublicKey(const std::string& publicKey) { publicKey_ = publicKey; }
    void setUpdateDirectory(const std::string& directory);
    
    // Update checking
    core::AsyncResult<UpdateInfo> checkForUpdates(bool force = false);
    void enableAutoCheck(bool enable, std::chrono::hours interval = std::chrono::hours(24));
    bool isAutoCheckEnabled() const { return autoCheckEnabled_.load(); }
    
    // Update downloading and installation
    core::AsyncResult<UpdateResult> downloadAndInstallUpdate(const UpdateInfo& updateInfo);
    core::AsyncResult<UpdateResult> downloadUpdate(const UpdateInfo& updateInfo);
    core::Result<UpdateResult> installDownloadedUpdate(const std::string& installerPath);
    
    // Update management
    void scheduleUpdate(const std::string& installerPath, bool installOnNextRestart = true);
    void cancelUpdate();
    bool isUpdateInProgress() const;
    UpdateStatus getCurrentStatus() const { return currentStatus_.load(); }
    
    // Callbacks
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = std::move(callback); }
    void setUpdateAvailableCallback(UpdateAvailableCallback callback) { updateAvailableCallback_ = std::move(callback); }
    void setUpdateCompleteCallback(UpdateCompleteCallback callback) { updateCompleteCallback_ = std::move(callback); }
    
    // Update history and rollback
    struct UpdateHistoryEntry {
        Version fromVersion;
        Version toVersion;
        std::chrono::system_clock::time_point updateTime;
        bool successful = false;
        std::string errorMessage;
    };
    
    std::vector<UpdateHistoryEntry> getUpdateHistory() const;
    bool canRollback() const;
    core::AsyncResult<UpdateResult> rollbackUpdate();
    
    // Utility
    Version getCurrentVersion() const { return currentVersion_; }
    UpdateChannel getUpdateChannel() const { return updateChannel_; }
    std::string getUpdateDirectory() const { return updateDirectory_; }
    
    // Statistics
    struct UpdateStats {
        int totalUpdateChecks = 0;
        int updatesAvailable = 0;
        int updatesDownloaded = 0;
        int updatesInstalled = 0;
        int updatesFailed = 0;
        std::chrono::system_clock::time_point lastCheck;
        std::chrono::system_clock::time_point lastUpdate;
        size_t totalBytesDownloaded = 0;
    };
    
    UpdateStats getUpdateStats() const;
    void resetUpdateStats();
    
private:
    // Update checking
    core::AsyncResult<UpdateInfo> fetchUpdateInfo();
    bool isUpdateAvailable(const UpdateInfo& updateInfo) const;
    void performAutoCheck();
    
    // Download management
    core::AsyncResult<std::string> downloadFile(
        const std::string& url, 
        const std::string& destinationPath,
        const std::string& expectedChecksum = "");
    
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                               curl_off_t ultotal, curl_off_t ulnow);
    
    // Verification
    bool verifySignature(const std::string& filePath, const std::string& signature);
    bool verifyChecksum(const std::string& filePath, const std::string& expectedChecksum);
    std::string calculateChecksum(const std::string& filePath);
    
    // Installation
    UpdateResult installUpdate(const std::string& installerPath);
    bool createBackup();
    bool restoreBackup();
    void cleanupTempFiles();
    
    // File system operations
    bool ensureUpdateDirectory();
    std::string getInstallerPath(const UpdateInfo& updateInfo) const;
    std::string getBackupPath() const;
    bool hasWritePermissions(const std::string& directory) const;
    size_t getAvailableDiskSpace(const std::string& path) const;
    
    // Network operations  
    struct HttpResponse {
        int statusCode;
        std::string body;
        std::map<std::string, std::string> headers;
    };
    
    core::AsyncResult<HttpResponse> makeHttpRequest(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers = {}
    );
    
    // Auto-check thread
    void startAutoCheckThread();
    void stopAutoCheckThread();
    void autoCheckLoop();
    
    // Update state persistence
    void saveUpdateState();
    void loadUpdateState();
    void saveUpdateHistory(const UpdateHistoryEntry& entry);
    
    // Configuration
    std::string updateServerUrl_ = "https://api.mixmindai.com/updates";
    UpdateChannel updateChannel_ = UpdateChannel::STABLE;
    Version currentVersion_;
    std::string publicKey_; // For signature verification
    std::string updateDirectory_;
    
    // Update state
    std::atomic<UpdateStatus> currentStatus_{UpdateStatus::IDLE};
    std::atomic<bool> updateInProgress_{false};
    std::atomic<bool> cancellationRequested_{false};
    
    // Auto-check configuration
    std::atomic<bool> autoCheckEnabled_{true};
    std::chrono::hours autoCheckInterval_{24};
    std::chrono::system_clock::time_point lastAutoCheck_;
    
    // Auto-check thread
    std::atomic<bool> autoCheckThreadRunning_{false};
    std::unique_ptr<std::thread> autoCheckThread_;
    
    // Callbacks
    ProgressCallback progressCallback_;
    UpdateAvailableCallback updateAvailableCallback_;
    UpdateCompleteCallback updateCompleteCallback_;
    
    // Download state
    struct DownloadProgress {
        size_t totalBytes = 0;
        size_t downloadedBytes = 0;
        std::chrono::system_clock::time_point startTime;
        std::string statusMessage;
    } downloadProgress_;
    
    mutable std::mutex progressMutex_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    UpdateStats stats_;
    
    // Update history
    mutable std::mutex historyMutex_;
    std::vector<UpdateHistoryEntry> updateHistory_;
    static constexpr int MAX_HISTORY_ENTRIES = 50;
    
    // Temporary file cleanup
    std::vector<std::string> tempFiles_;
    void addTempFile(const std::string& path);
};

// ============================================================================
// Update Notification UI Helper
// ============================================================================

class UpdateNotificationUI {
public:
    enum class NotificationAction {
        INSTALL_NOW,
        INSTALL_ON_RESTART,
        REMIND_LATER,
        SKIP_VERSION
    };
    
    struct UpdateDialog {
        std::string title;
        std::string message;
        std::string releaseNotes;
        std::vector<std::string> buttons;
        bool isCritical = false;
        
        static UpdateDialog createUpdateAvailableDialog(const AutoUpdater::UpdateInfo& updateInfo);
        static UpdateDialog createDownloadProgressDialog();
        static UpdateDialog createInstallDialog();
    };
    
    using DialogCallback = std::function<void(NotificationAction)>;
    
    static void showUpdateNotification(
        const AutoUpdater::UpdateInfo& updateInfo,
        DialogCallback callback
    );
    
    static void showProgressDialog(
        const std::string& title,
        const std::string& message,
        int progressPercent
    );
    
    static void hideProgressDialog();
    
private:
    static bool showingDialog_;
    static std::unique_ptr<void> dialogHandle_; // Platform-specific dialog handle
};

// ============================================================================
// Update Scheduler - Manages Update Installation Timing
// ============================================================================

class UpdateScheduler {
public:
    enum class ScheduleType {
        IMMEDIATE,          // Install right now
        ON_RESTART,         // Install when app restarts
        SCHEDULED_TIME,     // Install at specific time
        IDLE_TIME,          // Install when system is idle
        MAINTENANCE_WINDOW  // Install during maintenance window
    };
    
    struct ScheduleConfig {
        ScheduleType type = ScheduleType::ON_RESTART;
        std::chrono::system_clock::time_point scheduledTime;
        std::chrono::minutes maintenanceStart{120}; // 2 AM
        std::chrono::minutes maintenanceDuration{60}; // 1 hour
        std::chrono::minutes idleThreshold{15}; // 15 minutes idle
    };
    
    UpdateScheduler(AutoUpdater* updater);
    ~UpdateScheduler();
    
    void scheduleUpdate(const std::string& installerPath, const ScheduleConfig& config);
    void cancelScheduledUpdate();
    bool hasScheduledUpdate() const { return hasScheduledUpdate_.load(); }
    
    ScheduleConfig getScheduleConfig() const;
    std::chrono::system_clock::time_point getNextUpdateTime() const;
    
private:
    void startScheduleMonitoring();
    void stopScheduleMonitoring();
    void scheduleMonitoringLoop();
    
    bool isSystemIdle() const;
    bool isInMaintenanceWindow() const;
    void executeScheduledUpdate();
    
    AutoUpdater* updater_;
    std::string scheduledInstallerPath_;
    ScheduleConfig scheduleConfig_;
    
    std::atomic<bool> hasScheduledUpdate_{false};
    std::atomic<bool> monitoringThreadRunning_{false};
    std::unique_ptr<std::thread> monitoringThread_;
    
    mutable std::mutex scheduleMutex_;
};

// ============================================================================
// Global Auto-Updater Instance
// ============================================================================

AutoUpdater& getGlobalAutoUpdater();
void initializeAutoUpdater();
void shutdownAutoUpdater();

// Utility functions
std::string updateResultToString(AutoUpdater::UpdateResult result);
std::string updateStatusToString(AutoUpdater::UpdateStatus status);
std::string updateChannelToString(AutoUpdater::UpdateChannel channel);
AutoUpdater::UpdateChannel stringToUpdateChannel(const std::string& channel);

} // namespace mixmind::update