#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "../core/result.h"

namespace mixmind {

struct VST3PluginInfo {
    std::string name;
    std::string path;
    std::string manufacturer;
    std::string version;
    std::string uid;
    bool isValid = false;
};

class RealVST3Scanner {
public:
    RealVST3Scanner();
    ~RealVST3Scanner() = default;

    // Scan system VST3 directories
    Result<std::vector<VST3PluginInfo>> scanSystemPlugins();
    
    // Look for specific plugins (Span, TDR Nova)
    Result<VST3PluginInfo> findSpanPlugin();
    Result<VST3PluginInfo> findTDRNovaPlugin();
    
    // Validate plugin at specific path
    Result<VST3PluginInfo> validatePlugin(const std::filesystem::path& pluginPath);
    
    // Get system VST3 directories
    std::vector<std::filesystem::path> getSystemVST3Directories();
    
    // Print download instructions for missing plugins
    void printDownloadInstructions();

private:
    std::vector<std::filesystem::path> m_systemDirs;
    
    // Helper methods
    bool isValidVST3Bundle(const std::filesystem::path& path);
    Result<VST3PluginInfo> extractPluginInfo(const std::filesystem::path& pluginPath);
    std::string extractPluginName(const std::filesystem::path& pluginPath);
};