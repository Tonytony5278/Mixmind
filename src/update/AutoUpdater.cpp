#include "AutoUpdater.h"
#include "../core/logging.h"
#include <json/json.h>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace mixmind::update {

namespace fs = std::filesystem;
static std::unique_ptr<AutoUpdater> g_autoUpdater;

// ============================================================================
// Version Implementation
// ============================================================================

AutoUpdater::Version::Version(const std::string& versionString) {
    std::regex versionRegex(R"((\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?)");
    std::smatch matches;
    
    if (std::regex_match(versionString, matches, versionRegex)) {
        major = std::stoi(matches[1].str());
        minor = std::stoi(matches[2].str());
        patch = std::stoi(matches[3].str());
        if (matches[4].matched) {
            prerelease = matches[4].str();
        }
    } else {
        // Invalid version string
        major = minor = patch = -1;
    }
}

std::string AutoUpdater::Version::toString() const {
    std::string result = std::to_string(major) + "." + 
                        std::to_string(minor) + "." + 
                        std::to_string(patch);
    
    if (!prerelease.empty()) {
        result += "-" + prerelease;
    }
    
    return result;
}

bool AutoUpdater::Version::isNewerThan(const Version& other) const {
    if (major != other.major) return major > other.major;
    if (minor != other.minor) return minor > other.minor;
    if (patch != other.patch) return patch > other.patch;
    
    // Handle prerelease versions (prerelease < stable)
    if (prerelease.empty() && !other.prerelease.empty()) return true;
    if (!prerelease.empty() && other.prerelease.empty()) return false;
    
    return prerelease > other.prerelease;
}

bool AutoUpdater::Version::operator==(const Version& other) const {
    return major == other.major && 
           minor == other.minor && 
           patch == other.patch &&
           prerelease == other.prerelease;
}

// ============================================================================
// AutoUpdater Implementation
// ============================================================================

AutoUpdater::AutoUpdater() {
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Set default update directory
#ifdef _WIN32
    wchar_t* appDataPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appDataPath))) {
        updateDirectory_ = std::filesystem::path(appDataPath) / "MixMindAI" / "Updates";
        CoTaskMemFree(appDataPath);
    } else {
        updateDirectory_ = "Updates";
    }
#else
    updateDirectory_ = "Updates";
#endif
    
    ensureUpdateDirectory();
    loadUpdateState();
    
    LOG_INFO("AutoUpdater initialized");
}

AutoUpdater::~AutoUpdater() {
    stopAutoCheckThread();
    cleanupTempFiles();
    saveUpdateState();
    curl_global_cleanup();
}

void AutoUpdater::setUpdateDirectory(const std::string& directory) {
    updateDirectory_ = directory;
    ensureUpdateDirectory();
}

core::AsyncResult<AutoUpdater::UpdateInfo> AutoUpdater::checkForUpdates(bool force) {
    return core::async([this, force]() -> UpdateInfo {
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.totalUpdateChecks++;
            stats_.lastCheck = std::chrono::system_clock::now();
        }
        
        currentStatus_.store(UpdateStatus::CHECKING);
        
        if (progressCallback_) {
            progressCallback_(0, 0, "Checking for updates...");
        }
        
        auto updateInfo = fetchUpdateInfo();
        if (!updateInfo) {
            currentStatus_.store(UpdateStatus::FAILED);
            throw std::runtime_error("Failed to check for updates: " + updateInfo.error());
        }
        
        auto info = updateInfo.get();
        
        if (isUpdateAvailable(info)) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.updatesAvailable++;
            
            if (updateAvailableCallback_) {
                updateAvailableCallback_(info);
            }
            
            LOG_INFO("Update available: " + info.version.toString());
        } else {
            LOG_INFO("No updates available");
        }
        
        currentStatus_.store(UpdateStatus::IDLE);
        return info;
    });
}

core::AsyncResult<AutoUpdater::UpdateInfo> AutoUpdater::fetchUpdateInfo() {
    return core::async([this]() -> UpdateInfo {
        std::string endpoint = "/latest";
        
        // Add query parameters
        std::string channelStr;
        switch (updateChannel_) {
            case UpdateChannel::STABLE: channelStr = "stable"; break;
            case UpdateChannel::BETA: channelStr = "beta"; break;
            case UpdateChannel::ALPHA: channelStr = "alpha"; break;
        }
        
        endpoint += "?channel=" + channelStr + 
                   "&version=" + currentVersion_.toString();
        
        auto response = makeHttpRequest("GET", updateServerUrl_ + endpoint);
        if (!response) {
            throw std::runtime_error("Network request failed: " + response.error());
        }
        
        auto httpResponse = response.get();
        if (httpResponse.statusCode != 200) {
            throw std::runtime_error("Server returned error: " + std::to_string(httpResponse.statusCode));
        }
        
        // Parse JSON response
        Json::Value responseJson;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream responseStream(httpResponse.body);
        
        if (!Json::parseFromStream(builder, responseStream, &responseJson, &errors)) {
            throw std::runtime_error("Failed to parse update response: " + errors);
        }
        
        // Extract update information
        UpdateInfo updateInfo;
        updateInfo.version = Version(responseJson["version"].asString());
        updateInfo.downloadUrl = responseJson["download_url"].asString();
        updateInfo.changelogUrl = responseJson["changelog_url"].asString();
        updateInfo.releaseNotes = responseJson["release_notes"].asString();
        updateInfo.downloadSizeBytes = responseJson["download_size"].asUInt64();
        updateInfo.signature = responseJson["signature"].asString();
        updateInfo.checksum = responseJson["checksum"].asString();
        updateInfo.isCritical = responseJson["is_critical"].asBool();
        updateInfo.requiresRestart = responseJson["requires_restart"].asBool();
        
        // Parse release date
        auto releaseDateStr = responseJson["release_date"].asString();
        if (!releaseDateStr.empty()) {
            // Assuming ISO 8601 format: "2024-01-15T10:30:00Z"
            std::tm tm = {};
            std::istringstream ss(releaseDateStr);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            updateInfo.releaseDate = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }
        
        if (!updateInfo.isValid()) {
            throw std::runtime_error("Invalid update information received");
        }
        
        return updateInfo;
    });
}

bool AutoUpdater::isUpdateAvailable(const UpdateInfo& updateInfo) const {
    return updateInfo.version.isNewerThan(currentVersion_);
}

void AutoUpdater::enableAutoCheck(bool enable, std::chrono::hours interval) {
    autoCheckEnabled_.store(enable);
    autoCheckInterval_ = interval;
    
    if (enable) {
        startAutoCheckThread();
    } else {
        stopAutoCheckThread();
    }
}

void AutoUpdater::startAutoCheckThread() {
    if (autoCheckThreadRunning_.load()) return;
    
    autoCheckThreadRunning_.store(true);
    autoCheckThread_ = std::make_unique<std::thread>(&AutoUpdater::autoCheckLoop, this);
    
    LOG_INFO("Auto-update checking started");
}

void AutoUpdater::stopAutoCheckThread() {
    if (!autoCheckThreadRunning_.load()) return;
    
    autoCheckThreadRunning_.store(false);
    
    if (autoCheckThread_ && autoCheckThread_->joinable()) {
        autoCheckThread_->join();
    }
    
    autoCheckThread_.reset();
    
    LOG_INFO("Auto-update checking stopped");
}

void AutoUpdater::autoCheckLoop() {
    while (autoCheckThreadRunning_.load()) {
        try {
            auto now = std::chrono::system_clock::now();
            auto timeSinceLastCheck = now - lastAutoCheck_;
            
            if (timeSinceLastCheck >= autoCheckInterval_) {
                performAutoCheck();
                lastAutoCheck_ = now;
            }
            
            // Sleep for a short while before checking again
            std::this_thread::sleep_for(std::chrono::minutes(30));
            
        } catch (const std::exception& e) {
            LOG_ERROR("Auto-update check error: " + std::string(e.what()));
        }
    }
}

void AutoUpdater::performAutoCheck() {
    if (updateInProgress_.load()) {
        return; // Skip if update already in progress
    }
    
    try {
        auto updateInfoFuture = checkForUpdates(false);
        
        // Don't block the auto-check thread
        std::thread([this, updateInfoFuture = std::move(updateInfoFuture)]() mutable {
            try {
                auto updateInfo = updateInfoFuture.get();
                
                if (isUpdateAvailable(updateInfo)) {
                    LOG_INFO("Auto-check found update: " + updateInfo.version.toString());
                    
                    if (updateAvailableCallback_) {
                        updateAvailableCallback_(updateInfo);
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Auto-check failed: " + std::string(e.what()));
            }
        }).detach();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Auto-check initialization failed: " + std::string(e.what()));
    }
}

core::AsyncResult<AutoUpdater::UpdateResult> AutoUpdater::downloadAndInstallUpdate(const UpdateInfo& updateInfo) {
    return core::async([this, updateInfo]() -> UpdateResult {
        if (updateInProgress_.load()) {
            return UpdateResult::USER_CANCELLED;
        }
        
        updateInProgress_.store(true);
        cancellationRequested_.store(false);
        
        try {
            // Download update
            auto downloadResult = downloadUpdate(updateInfo);
            if (!downloadResult) {
                updateInProgress_.store(false);
                return UpdateResult::DOWNLOAD_FAILED;
            }
            
            auto result = downloadResult.get();
            if (result != UpdateResult::SUCCESS) {
                updateInProgress_.store(false);
                return result;
            }
            
            // Install update
            std::string installerPath = getInstallerPath(updateInfo);
            auto installResult = installDownloadedUpdate(installerPath);
            
            updateInProgress_.store(false);
            
            if (installResult) {
                auto finalResult = *installResult;
                
                // Record update history
                UpdateHistoryEntry historyEntry;
                historyEntry.fromVersion = currentVersion_;
                historyEntry.toVersion = updateInfo.version;
                historyEntry.updateTime = std::chrono::system_clock::now();
                historyEntry.successful = (finalResult == UpdateResult::SUCCESS);
                
                if (finalResult != UpdateResult::SUCCESS) {
                    historyEntry.errorMessage = updateResultToString(finalResult);
                }
                
                saveUpdateHistory(historyEntry);
                
                if (updateCompleteCallback_) {
                    updateCompleteCallback_(finalResult, historyEntry.errorMessage);
                }
                
                return finalResult;
            } else {
                return UpdateResult::INSTALLATION_FAILED;
            }
            
        } catch (const std::exception& e) {
            updateInProgress_.store(false);
            LOG_ERROR("Update failed: " + std::string(e.what()));
            
            if (updateCompleteCallback_) {
                updateCompleteCallback_(UpdateResult::INSTALLATION_FAILED, e.what());
            }
            
            return UpdateResult::INSTALLATION_FAILED;
        }
    });
}

core::AsyncResult<AutoUpdater::UpdateResult> AutoUpdater::downloadUpdate(const UpdateInfo& updateInfo) {
    return core::async([this, updateInfo]() -> UpdateResult {
        currentStatus_.store(UpdateStatus::DOWNLOADING);
        
        // Check disk space
        size_t availableSpace = getAvailableDiskSpace(updateDirectory_);
        if (availableSpace < updateInfo.downloadSizeBytes * 2) { // 2x for safety
            return UpdateResult::DISK_SPACE_INSUFFICIENT;
        }
        
        std::string installerPath = getInstallerPath(updateInfo);
        
        {
            std::lock_guard<std::mutex> lock(progressMutex_);
            downloadProgress_.totalBytes = updateInfo.downloadSizeBytes;
            downloadProgress_.downloadedBytes = 0;
            downloadProgress_.startTime = std::chrono::system_clock::now();
            downloadProgress_.statusMessage = "Downloading update...";
        }
        
        auto downloadResult = downloadFile(updateInfo.downloadUrl, installerPath, updateInfo.checksum);
        if (!downloadResult) {
            return UpdateResult::DOWNLOAD_FAILED;
        }
        
        std::string downloadedFile = downloadResult.get();
        addTempFile(downloadedFile);
        
        currentStatus_.store(UpdateStatus::VERIFYING);
        
        if (progressCallback_) {
            progressCallback_(100, 100, "Verifying download...");
        }
        
        // Verify signature
        if (!verifySignature(downloadedFile, updateInfo.signature)) {
            return UpdateResult::VERIFICATION_FAILED;
        }
        
        // Verify checksum
        if (!verifyChecksum(downloadedFile, updateInfo.checksum)) {
            return UpdateResult::VERIFICATION_FAILED;
        }
        
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.updatesDownloaded++;
            stats_.totalBytesDownloaded += updateInfo.downloadSizeBytes;
        }
        
        currentStatus_.store(UpdateStatus::COMPLETED);
        
        LOG_INFO("Update downloaded and verified: " + downloadedFile);
        return UpdateResult::SUCCESS;
    });
}

core::AsyncResult<std::string> AutoUpdater::downloadFile(
    const std::string& url, 
    const std::string& destinationPath,
    const std::string& expectedChecksum) {
    
    return core::async([this, url, destinationPath, expectedChecksum]() -> std::string {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        std::ofstream outFile(destinationPath, std::ios::binary);
        if (!outFile) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("Failed to create output file: " + destinationPath);
        }
        
        // Set CURL options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minutes
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        
        CURLcode res = curl_easy_perform(curl);
        
        long statusCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
        
        curl_easy_cleanup(curl);
        outFile.close();
        
        if (res != CURLE_OK) {
            fs::remove(destinationPath);
            throw std::runtime_error("Download failed: " + std::string(curl_easy_strerror(res)));
        }
        
        if (statusCode != 200) {
            fs::remove(destinationPath);
            throw std::runtime_error("Download failed with HTTP " + std::to_string(statusCode));
        }
        
        // Verify checksum if provided
        if (!expectedChecksum.empty() && !verifyChecksum(destinationPath, expectedChecksum)) {
            fs::remove(destinationPath);
            throw std::runtime_error("Checksum verification failed");
        }
        
        return destinationPath;
    });
}

size_t AutoUpdater::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* outFile = static_cast<std::ofstream*>(userp);
    size_t totalSize = size * nmemb;
    outFile->write(static_cast<const char*>(contents), totalSize);
    return totalSize;
}

int AutoUpdater::progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                 curl_off_t ultotal, curl_off_t ulnow) {
    AutoUpdater* updater = static_cast<AutoUpdater*>(clientp);
    
    if (updater->cancellationRequested_.load()) {
        return 1; // Cancel download
    }
    
    if (dltotal > 0) {
        std::lock_guard<std::mutex> lock(updater->progressMutex_);
        updater->downloadProgress_.totalBytes = static_cast<size_t>(dltotal);
        updater->downloadProgress_.downloadedBytes = static_cast<size_t>(dlnow);
        
        if (updater->progressCallback_) {
            updater->progressCallback_(static_cast<size_t>(dlnow), 
                                     static_cast<size_t>(dltotal),
                                     "Downloading update...");
        }
    }
    
    return 0; // Continue
}

bool AutoUpdater::verifyChecksum(const std::string& filePath, const std::string& expectedChecksum) {
    std::string actualChecksum = calculateChecksum(filePath);
    return actualChecksum == expectedChecksum;
}

std::string AutoUpdater::calculateChecksum(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }
    
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    EVP_DigestInit_ex(mdctx, md, nullptr);
    
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        EVP_DigestUpdate(mdctx, buffer, file.gcount());
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;
    EVP_DigestFinal_ex(mdctx, hash, &hashLen);
    EVP_MD_CTX_free(mdctx);
    
    // Convert to hex string
    std::stringstream ss;
    for (unsigned int i = 0; i < hashLen; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

bool AutoUpdater::ensureUpdateDirectory() {
    try {
        fs::create_directories(updateDirectory_);
        return fs::exists(updateDirectory_) && hasWritePermissions(updateDirectory_);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create update directory: " + std::string(e.what()));
        return false;
    }
}

std::string AutoUpdater::getInstallerPath(const UpdateInfo& updateInfo) const {
    std::string filename = "MixMindAI_" + updateInfo.version.toString() + "_installer.exe";
    return (fs::path(updateDirectory_) / filename).string();
}

bool AutoUpdater::hasWritePermissions(const std::string& directory) const {
#ifdef _WIN32
    // Try to create a temporary file
    std::string testFile = (fs::path(directory) / "write_test.tmp").string();
    std::ofstream test(testFile);
    if (test) {
        test.close();
        fs::remove(testFile);
        return true;
    }
    return false;
#else
    return fs::is_directory(directory);
#endif
}

size_t AutoUpdater::getAvailableDiskSpace(const std::string& path) const {
    try {
        auto space = fs::space(path);
        return static_cast<size_t>(space.available);
    } catch (const std::exception&) {
        return 0;
    }
}

void AutoUpdater::addTempFile(const std::string& path) {
    tempFiles_.push_back(path);
}

void AutoUpdater::cleanupTempFiles() {
    for (const auto& path : tempFiles_) {
        try {
            if (fs::exists(path)) {
                fs::remove(path);
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to cleanup temp file " + path + ": " + e.what());
        }
    }
    tempFiles_.clear();
}

AutoUpdater::UpdateStats AutoUpdater::getUpdateStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

// ============================================================================
// Global Auto-Updater Instance
// ============================================================================

AutoUpdater& getGlobalAutoUpdater() {
    if (!g_autoUpdater) {
        throw std::runtime_error("AutoUpdater not initialized");
    }
    return *g_autoUpdater;
}

void initializeAutoUpdater() {
    if (!g_autoUpdater) {
        g_autoUpdater = std::make_unique<AutoUpdater>();
        g_autoUpdater->enableAutoCheck(true);
        LOG_INFO("Global AutoUpdater initialized");
    }
}

void shutdownAutoUpdater() {
    if (g_autoUpdater) {
        g_autoUpdater.reset();
        LOG_INFO("AutoUpdater system shutdown");
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string updateResultToString(AutoUpdater::UpdateResult result) {
    switch (result) {
        case AutoUpdater::UpdateResult::SUCCESS: return "Success";
        case AutoUpdater::UpdateResult::NO_UPDATE_AVAILABLE: return "No update available";
        case AutoUpdater::UpdateResult::DOWNLOAD_FAILED: return "Download failed";
        case AutoUpdater::UpdateResult::VERIFICATION_FAILED: return "Verification failed";
        case AutoUpdater::UpdateResult::INSTALLATION_FAILED: return "Installation failed";
        case AutoUpdater::UpdateResult::NETWORK_ERROR: return "Network error";
        case AutoUpdater::UpdateResult::PERMISSION_DENIED: return "Permission denied";
        case AutoUpdater::UpdateResult::DISK_SPACE_INSUFFICIENT: return "Insufficient disk space";
        case AutoUpdater::UpdateResult::USER_CANCELLED: return "User cancelled";
        default: return "Unknown error";
    }
}

std::string updateStatusToString(AutoUpdater::UpdateStatus status) {
    switch (status) {
        case AutoUpdater::UpdateStatus::IDLE: return "Idle";
        case AutoUpdater::UpdateStatus::CHECKING: return "Checking for updates";
        case AutoUpdater::UpdateStatus::DOWNLOADING: return "Downloading";
        case AutoUpdater::UpdateStatus::VERIFYING: return "Verifying";
        case AutoUpdater::UpdateStatus::INSTALLING: return "Installing";
        case AutoUpdater::UpdateStatus::COMPLETED: return "Completed";
        case AutoUpdater::UpdateStatus::FAILED: return "Failed";
        default: return "Unknown";
    }
}

std::string updateChannelToString(AutoUpdater::UpdateChannel channel) {
    switch (channel) {
        case AutoUpdater::UpdateChannel::STABLE: return "stable";
        case AutoUpdater::UpdateChannel::BETA: return "beta";
        case AutoUpdater::UpdateChannel::ALPHA: return "alpha";
        default: return "stable";
    }
}

} // namespace mixmind::update