#include "TEVSTScanner.h"
#include "../../core/async.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#elif __linux__
#include <unistd.h>
#include <pwd.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace mixmind::adapters::tracktion {

// ============================================================================
// TEVSTScanner Implementation  
// ============================================================================

TEVSTScanner::TEVSTScanner() {
    // Set default cache file location
#ifdef _WIN32
    char* appData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData != nullptr) {
        defaultCacheFile_ = std::string(appData) + "\\MixMindAI\\vst3_cache.json";
        free(appData);
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        defaultCacheFile_ = std::string(home) + "/.mixmind/vst3_cache.json";
    }
#endif
    
    // Try to load existing cache
    loadCache();
}

TEVSTScanner::~TEVSTScanner() {
    // Save cache on shutdown
    saveCache();
}

// ========================================================================
// Scanning Operations
// ========================================================================

core::AsyncResult<core::VoidResult> TEVSTScanner::scanAllDirectories() {
    return core::executeAsyncVoid([this]() -> core::VoidResult {
        auto directories = getDefaultVST3Directories();
        int totalDirs = static_cast<int>(directories.size());
        int currentDir = 0;
        
        for (const auto& dir : directories) {
            reportProgress("Scanning " + dir, static_cast<float>(currentDir) / totalDirs);
            
            auto result = scanDirectory(dir).get();
            if (result.isError()) {
                // Log error but continue with other directories
                std::cerr << "Failed to scan directory " << dir << ": " 
                          << result.error().toString() << std::endl;
            }
            
            currentDir++;
        }
        
        reportProgress("Scan complete", 1.0f);
        
        // Update cache stats
        std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
        lastStats_.lastScan = std::chrono::system_clock::now();
        lastStats_.totalPlugins = static_cast<int>(pluginDatabase_.size());
        lastStats_.validPlugins = 0;
        lastStats_.blacklistedPlugins = 0;
        lastStats_.failedScans = 0;
        
        for (const auto& plugin : pluginDatabase_) {
            if (plugin.isBlacklisted) {
                lastStats_.blacklistedPlugins++;
            } else if (plugin.scanSuccessful) {
                lastStats_.validPlugins++;
            } else {
                lastStats_.failedScans++;
            }
        }
        
        return core::VoidResult::success();
    }, "VST3 directory scan");
}

core::AsyncResult<core::VoidResult> TEVSTScanner::scanDirectory(const std::string& directoryPath) {
    return core::executeAsyncVoid([this, directoryPath]() -> core::VoidResult {
        if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
            return core::VoidResult::error(
                core::ErrorCode::FileNotFound,
                core::ErrorCategory::fileIO(),
                "Directory not found: " + directoryPath
            );
        }
        
        auto vst3Files = findVST3Files(directoryPath);
        int totalFiles = static_cast<int>(vst3Files.size());
        int currentFile = 0;
        
        for (const auto& filePath : vst3Files) {
            reportProgress("Scanning " + fs::path(filePath).filename().string(), 
                         static_cast<float>(currentFile) / totalFiles);
            
            auto result = scanPluginFile(filePath).get();
            if (result.isSuccess()) {
                std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
                
                // Check if plugin already exists (update or add)
                auto existing = std::find_if(pluginDatabase_.begin(), pluginDatabase_.end(),
                    [&](const VST3PluginInfo& plugin) {
                        return plugin.uid == result.value().uid;
                    });
                
                if (existing != pluginDatabase_.end()) {
                    *existing = result.value(); // Update existing
                } else {
                    pluginDatabase_.push_back(result.value()); // Add new
                }
            }
            
            currentFile++;
        }
        
        return core::VoidResult::success();
    }, "VST3 directory scan: " + directoryPath);
}

core::AsyncResult<core::Result<VST3PluginInfo>> TEVSTScanner::scanPluginFile(const std::string& filePath) {
    return core::executeAsync<VST3PluginInfo>([this, filePath]() -> core::Result<VST3PluginInfo> {
        return scanVST3File(filePath);
    }, "VST3 plugin scan: " + filePath);
}

core::AsyncResult<core::VoidResult> TEVSTScanner::rescanAll() {
    return core::executeAsyncVoid([this]() -> core::VoidResult {
        std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
        
        // Clear current database
        pluginDatabase_.clear();
        
        // Re-scan all directories
        return scanAllDirectories().get();
    }, "VST3 rescan all");
}

// ========================================================================
// Plugin Database
// ========================================================================

std::vector<VST3PluginInfo> TEVSTScanner::getAllPlugins() const {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    return pluginDatabase_;
}

std::vector<VST3PluginInfo> TEVSTScanner::getPluginsByCategory(const std::string& category) const {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    
    std::vector<VST3PluginInfo> result;
    std::copy_if(pluginDatabase_.begin(), pluginDatabase_.end(), std::back_inserter(result),
        [&category](const VST3PluginInfo& plugin) {
            return plugin.category == category && plugin.isValid();
        });
    
    return result;
}

std::optional<VST3PluginInfo> TEVSTScanner::findPluginByUID(const std::string& uid) const {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    
    auto it = std::find_if(pluginDatabase_.begin(), pluginDatabase_.end(),
        [&uid](const VST3PluginInfo& plugin) {
            return plugin.uid == uid;
        });
    
    if (it != pluginDatabase_.end()) {
        return *it;
    }
    
    return std::nullopt;
}

std::vector<VST3PluginInfo> TEVSTScanner::findPluginsByName(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    
    std::vector<VST3PluginInfo> result;
    std::copy_if(pluginDatabase_.begin(), pluginDatabase_.end(), std::back_inserter(result),
        [&name](const VST3PluginInfo& plugin) {
            return plugin.name.find(name) != std::string::npos && plugin.isValid();
        });
    
    return result;
}

// ========================================================================
// Blacklist Management
// ========================================================================

void TEVSTScanner::blacklistPlugin(const std::string& uid, const std::string& reason) {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    
    blacklistedPlugins_[uid] = reason.empty() ? "User blacklisted" : reason;
    
    // Update plugin in database
    auto it = std::find_if(pluginDatabase_.begin(), pluginDatabase_.end(),
        [&uid](VST3PluginInfo& plugin) {
            return plugin.uid == uid;
        });
    
    if (it != pluginDatabase_.end()) {
        it->isBlacklisted = true;
        it->scanError = reason.empty() ? "Blacklisted by user" : reason;
    }
}

void TEVSTScanner::unblacklistPlugin(const std::string& uid) {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    
    blacklistedPlugins_.erase(uid);
    
    // Update plugin in database
    auto it = std::find_if(pluginDatabase_.begin(), pluginDatabase_.end(),
        [&uid](VST3PluginInfo& plugin) {
            return plugin.uid == uid;
        });
    
    if (it != pluginDatabase_.end()) {
        it->isBlacklisted = false;
        it->scanError.clear();
    }
}

bool TEVSTScanner::isPluginBlacklisted(const std::string& uid) const {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    return blacklistedPlugins_.find(uid) != blacklistedPlugins_.end();
}

std::vector<std::string> TEVSTScanner::getBlacklistedPlugins() const {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    
    std::vector<std::string> result;
    for (const auto& [uid, reason] : blacklistedPlugins_) {
        result.push_back(uid);
    }
    
    return result;
}

// ========================================================================
// Cache Management
// ========================================================================

core::VoidResult TEVSTScanner::saveCache(const std::string& cacheFilePath) const {
    std::string filePath = cacheFilePath.empty() ? defaultCacheFile_ : cacheFilePath;
    
    try {
        // Create directory if it doesn't exist
        fs::create_directories(fs::path(filePath).parent_path());
        
        json cacheData;
        
        {
            std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
            
            // Save plugins
            json pluginsArray = json::array();
            for (const auto& plugin : pluginDatabase_) {
                json pluginJson;
                pluginJson["name"] = plugin.name;
                pluginJson["vendor"] = plugin.vendor;
                pluginJson["category"] = plugin.category;
                pluginJson["version"] = plugin.version;
                pluginJson["uid"] = plugin.uid;
                pluginJson["filePath"] = plugin.filePath;
                pluginJson["hasEditor"] = plugin.hasEditor;
                pluginJson["isSynth"] = plugin.isSynth;
                pluginJson["isEffect"] = plugin.isEffect;
                pluginJson["numAudioInputs"] = plugin.numAudioInputs;
                pluginJson["numAudioOutputs"] = plugin.numAudioOutputs;
                pluginJson["numMidiInputs"] = plugin.numMidiInputs;
                pluginJson["numMidiOutputs"] = plugin.numMidiOutputs;
                pluginJson["isBlacklisted"] = plugin.isBlacklisted;
                pluginJson["scanSuccessful"] = plugin.scanSuccessful;
                pluginJson["scanError"] = plugin.scanError;
                
                auto time_t = std::chrono::system_clock::to_time_t(plugin.lastScanned);
                pluginJson["lastScanned"] = time_t;
                
                pluginsArray.push_back(pluginJson);
            }
            cacheData["plugins"] = pluginsArray;
            
            // Save blacklist
            json blacklistJson;
            for (const auto& [uid, reason] : blacklistedPlugins_) {
                blacklistJson[uid] = reason;
            }
            cacheData["blacklist"] = blacklistJson;
            
            // Save metadata
            cacheData["version"] = "1.0";
            cacheData["lastUpdated"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        }
        
        // Write to file
        std::ofstream file(filePath);
        file << cacheData.dump(2);
        file.close();
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::FileAccessDenied,
            core::ErrorCategory::fileIO(),
            "Failed to save VST3 cache: " + std::string(e.what())
        );
    }
}

core::VoidResult TEVSTScanner::loadCache(const std::string& cacheFilePath) {
    std::string filePath = cacheFilePath.empty() ? defaultCacheFile_ : cacheFilePath;
    
    if (!fs::exists(filePath)) {
        return core::VoidResult::success(); // No cache file is not an error
    }
    
    try {
        std::ifstream file(filePath);
        json cacheData;
        file >> cacheData;
        
        std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
        
        // Load plugins
        if (cacheData.contains("plugins")) {
            pluginDatabase_.clear();
            for (const auto& pluginJson : cacheData["plugins"]) {
                VST3PluginInfo plugin;
                plugin.name = pluginJson.value("name", "");
                plugin.vendor = pluginJson.value("vendor", "");
                plugin.category = pluginJson.value("category", "");
                plugin.version = pluginJson.value("version", "");
                plugin.uid = pluginJson.value("uid", "");
                plugin.filePath = pluginJson.value("filePath", "");
                plugin.hasEditor = pluginJson.value("hasEditor", false);
                plugin.isSynth = pluginJson.value("isSynth", false);
                plugin.isEffect = pluginJson.value("isEffect", true);
                plugin.numAudioInputs = pluginJson.value("numAudioInputs", 2);
                plugin.numAudioOutputs = pluginJson.value("numAudioOutputs", 2);
                plugin.numMidiInputs = pluginJson.value("numMidiInputs", 1);
                plugin.numMidiOutputs = pluginJson.value("numMidiOutputs", 0);
                plugin.isBlacklisted = pluginJson.value("isBlacklisted", false);
                plugin.scanSuccessful = pluginJson.value("scanSuccessful", true);
                plugin.scanError = pluginJson.value("scanError", "");
                
                if (pluginJson.contains("lastScanned")) {
                    auto time_t = pluginJson["lastScanned"].get<std::time_t>();
                    plugin.lastScanned = std::chrono::system_clock::from_time_t(time_t);
                }
                
                pluginDatabase_.push_back(plugin);
            }
        }
        
        // Load blacklist
        if (cacheData.contains("blacklist")) {
            blacklistedPlugins_.clear();
            for (const auto& [uid, reason] : cacheData["blacklist"].items()) {
                blacklistedPlugins_[uid] = reason.get<std::string>();
            }
        }
        
        return core::VoidResult::success();
        
    } catch (const std::exception& e) {
        return core::VoidResult::error(
            core::ErrorCode::FileCorrupted,
            core::ErrorCategory::fileIO(),
            "Failed to load VST3 cache: " + std::string(e.what())
        );
    }
}

void TEVSTScanner::clearCache() {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    pluginDatabase_.clear();
    blacklistedPlugins_.clear();
}

TEVSTScanner::CacheStats TEVSTScanner::getCacheStats() const {
    std::lock_guard<std::recursive_mutex> lock(databaseMutex_);
    return lastStats_;
}

// ========================================================================
// Progress Reporting
// ========================================================================

void TEVSTScanner::setProgressCallback(std::function<void(const std::string&, float)> callback) {
    progressCallback_ = callback;
}

// ========================================================================
// Private Implementation
// ========================================================================

std::vector<std::string> TEVSTScanner::getDefaultVST3Directories() const {
    std::vector<std::string> directories;
    
#ifdef _WIN32
    // Windows VST3 locations
    char* programFiles = nullptr;
    size_t len = 0;
    if (_dupenv_s(&programFiles, &len, "PROGRAMFILES") == 0 && programFiles != nullptr) {
        directories.push_back(std::string(programFiles) + "\\Common Files\\VST3");
        free(programFiles);
    }
    
    char* programFilesX86 = nullptr;
    if (_dupenv_s(&programFilesX86, &len, "PROGRAMFILES(X86)") == 0 && programFilesX86 != nullptr) {
        directories.push_back(std::string(programFilesX86) + "\\Common Files\\VST3");
        free(programFilesX86);
    }
    
#elif __APPLE__
    // macOS VST3 locations
    directories.push_back("/Library/Audio/Plug-Ins/VST3");
    directories.push_back("/System/Library/Audio/Plug-Ins/VST3");
    
    const char* home = getenv("HOME");
    if (home) {
        directories.push_back(std::string(home) + "/Library/Audio/Plug-Ins/VST3");
    }
    
#elif __linux__
    // Linux VST3 locations
    directories.push_back("/usr/lib/vst3");
    directories.push_back("/usr/local/lib/vst3");
    
    const char* home = getenv("HOME");
    if (home) {
        directories.push_back(std::string(home) + "/.vst3");
    }
#endif
    
    return directories;
}

std::vector<std::string> TEVSTScanner::findVST3Files(const std::string& directoryPath) const {
    std::vector<std::string> vst3Files;
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(directoryPath)) {
            if (entry.is_regular_file() || entry.is_directory()) {
                std::string path = entry.path().string();
                
                // VST3 plugins can be files (.vst3) or bundles (.vst3 directories)
                if (path.ends_with(".vst3")) {
                    vst3Files.push_back(path);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory " << directoryPath << ": " << e.what() << std::endl;
    }
    
    return vst3Files;
}

core::Result<VST3PluginInfo> TEVSTScanner::scanVST3File(const std::string& filePath) const {
    VST3PluginInfo info;
    info.filePath = filePath;
    info.lastScanned = std::chrono::system_clock::now();
    info.scanSuccessful = false;
    
    try {
        // Load VST3 module
        auto module = VST::Hosting::Module::create(filePath, std::string());
        if (!module) {
            info.scanError = "Failed to load VST3 module";
            return core::Result<VST3PluginInfo>::success(std::move(info));
        }
        
        // Get plugin factory
        auto factory = module->getFactory();
        if (!factory.get()) {
            info.scanError = "Failed to get plugin factory";
            return core::Result<VST3PluginInfo>::success(std::move(info));
        }
        
        // Extract plugin information
        return extractPluginInfo(filePath, module, factory);
        
    } catch (const std::exception& e) {
        info.scanError = "Exception during scan: " + std::string(e.what());
        return core::Result<VST3PluginInfo>::success(std::move(info));
    } catch (...) {
        info.scanError = "Unknown exception during scan";
        return core::Result<VST3PluginInfo>::success(std::move(info));
    }
}

core::Result<VST3PluginInfo> TEVSTScanner::extractPluginInfo(
    const std::string& filePath,
    VST::Hosting::Module::Ptr module,
    const VST::Hosting::PluginFactory& factory) const {
    
    VST3PluginInfo info;
    info.filePath = filePath;
    info.lastScanned = std::chrono::system_clock::now();
    info.scanSuccessful = false;
    
    // Get first plugin from factory (most VST3s have only one)
    for (auto& classInfo : factory.classInfos()) {
        if (classInfo.category() == kVstAudioEffectClass) {
            info.name = classInfo.name();
            info.vendor = classInfo.vendor();
            info.category = classInfo.subCategories();
            info.version = classInfo.version();
            
            // Create UID string from TUID
            char uidStr[33];
            FUID::fromTUID(classInfo.ID().data()).toString(uidStr);
            info.uid = uidStr;
            
            // Try to create component to get more detailed info
            auto component = factory.createInstance<Steinberg::Vst::IComponent>(classInfo.ID());
            if (component) {
                // Initialize component
                if (component->initialize(VST::Hosting::HostApplication::instance()) == kResultOk) {
                    
                    // Check for editor
                    auto editController = VST::Hosting::cast<Steinberg::Vst::IEditController>(component.get());
                    if (editController) {
                        info.hasEditor = true;
                    }
                    
                    // Get bus information
                    int32 numInputBuses = component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
                    int32 numOutputBuses = component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
                    
                    if (numInputBuses > 0) {
                        Steinberg::Vst::BusInfo busInfo;
                        if (component->getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, 0, busInfo) == kResultOk) {
                            info.numAudioInputs = busInfo.channelCount;
                        }
                    }
                    
                    if (numOutputBuses > 0) {
                        Steinberg::Vst::BusInfo busInfo;
                        if (component->getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, 0, busInfo) == kResultOk) {
                            info.numAudioOutputs = busInfo.channelCount;
                        }
                    }
                    
                    // Check if it's a synthesizer
                    info.isSynth = (numInputBuses == 0 && numOutputBuses > 0);
                    info.isEffect = !info.isSynth;
                    
                    component->terminate();
                }
            }
            
            info.scanSuccessful = true;
            break; // Take first valid plugin
        }
    }
    
    if (!info.scanSuccessful) {
        info.scanError = "No valid audio effect class found";
    }
    
    // Check if blacklisted
    if (isPluginBlacklisted(info.uid)) {
        info.isBlacklisted = true;
    }
    
    return core::Result<VST3PluginInfo>::success(std::move(info));
}

void TEVSTScanner::reportProgress(const std::string& message, float progress) {
    if (progressCallback_) {
        progressCallback_(message, progress);
    }
}

// ============================================================================
// Global Scanner Instance
// ============================================================================

TEVSTScanner& getGlobalVSTScanner() {
    static TEVSTScanner instance;
    return instance;
}

} // namespace mixmind::adapters::tracktion