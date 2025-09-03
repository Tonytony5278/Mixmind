#include "RealVST3Scanner.h"
#include <iostream>
#include <sstream>

namespace mixmind {

RealVST3Scanner::RealVST3Scanner() {
    // Initialize system VST3 directories
    m_systemDirs = getSystemVST3Directories();
}

std::vector<std::filesystem::path> RealVST3Scanner::getSystemVST3Directories() {
    std::vector<std::filesystem::path> dirs;
    
#ifdef _WIN32
    // Windows VST3 directories
    dirs.push_back("C:/Program Files/Common Files/VST3");
    dirs.push_back("C:/Program Files (x86)/Common Files/VST3");
    
    // User-specific directories
    auto userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        std::string userPath = userProfile;
        dirs.push_back(userPath + "/AppData/Roaming/VST3");
        dirs.push_back(userPath + "/Documents/VST3");
    }
#elif __APPLE__
    dirs.push_back("/Library/Audio/Plug-Ins/VST3");
    dirs.push_back("~/Library/Audio/Plug-Ins/VST3");
    dirs.push_back("/System/Library/Audio/Plug-Ins/VST3");
#else
    // Linux VST3 directories
    dirs.push_back("/usr/lib/vst3");
    dirs.push_back("/usr/local/lib/vst3");
    dirs.push_back("~/.vst3");
#endif

    return dirs;
}

Result<std::vector<VST3PluginInfo>> RealVST3Scanner::scanSystemPlugins() {
    std::vector<VST3PluginInfo> plugins;
    
    std::cout << "Scanning VST3 directories for plugins...\n";
    
    for (const auto& dir : m_systemDirs) {
        if (!std::filesystem::exists(dir)) {
            std::cout << "Directory not found: " << dir << "\n";
            continue;
        }
        
        std::cout << "Scanning: " << dir << "\n";
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_directory() && isValidVST3Bundle(entry.path())) {
                    auto pluginResult = extractPluginInfo(entry.path());
                    if (pluginResult.isSuccess()) {
                        plugins.push_back(pluginResult.getValue());
                        std::cout << "  Found: " << pluginResult.getValue().name << "\n";
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cout << "Error scanning " << dir << ": " << e.what() << "\n";
        }
    }
    
    if (plugins.empty()) {
        return Result<std::vector<VST3PluginInfo>>::error("No VST3 plugins found in system directories");
    }
    
    return Result<std::vector<VST3PluginInfo>>::success(plugins);
}

Result<VST3PluginInfo> RealVST3Scanner::findSpanPlugin() {
    std::cout << "Looking for Span.vst3...\n";
    
    // Common paths for Span
    std::vector<std::string> spanPaths = {
        "C:/Program Files/Common Files/VST3/Span.vst3",
        "C:/Program Files (x86)/Common Files/VST3/Span.vst3"
    };
    
    for (const auto& path : spanPaths) {
        if (std::filesystem::exists(path)) {
            std::cout << "Found Span at: " << path << "\n";
            return validatePlugin(path);
        }
    }
    
    return Result<VST3PluginInfo>::error("Span.vst3 not found");
}

Result<VST3PluginInfo> RealVST3Scanner::findTDRNovaPlugin() {
    std::cout << "Looking for TDR Nova.vst3...\n";
    
    // Common paths for TDR Nova
    std::vector<std::string> novaPaths = {
        "C:/Program Files/Common Files/VST3/TDR Nova.vst3",
        "C:/Program Files (x86)/Common Files/VST3/TDR Nova.vst3",
        "C:/Program Files/Common Files/VST3/Tokyo Dawn Labs/TDR Nova.vst3"
    };
    
    for (const auto& path : novaPaths) {
        if (std::filesystem::exists(path)) {
            std::cout << "Found TDR Nova at: " << path << "\n";
            return validatePlugin(path);
        }
    }
    
    return Result<VST3PluginInfo>::error("TDR Nova.vst3 not found");
}

Result<VST3PluginInfo> RealVST3Scanner::validatePlugin(const std::filesystem::path& pluginPath) {
    if (!std::filesystem::exists(pluginPath)) {
        return Result<VST3PluginInfo>::error("Plugin file does not exist: " + pluginPath.string());
    }
    
    if (!isValidVST3Bundle(pluginPath)) {
        return Result<VST3PluginInfo>::error("Invalid VST3 bundle: " + pluginPath.string());
    }
    
    return extractPluginInfo(pluginPath);
}

bool RealVST3Scanner::isValidVST3Bundle(const std::filesystem::path& path) {
    // VST3 plugins are bundles (directories) ending in .vst3
    return std::filesystem::is_directory(path) && 
           path.extension().string() == ".vst3";
}

Result<VST3PluginInfo> RealVST3Scanner::extractPluginInfo(const std::filesystem::path& pluginPath) {
    VST3PluginInfo info;
    info.path = pluginPath.string();
    info.name = extractPluginName(pluginPath);
    info.isValid = true;
    
    // Look for Contents/x86_64-win/ or similar architecture-specific directories
    auto contentsDir = pluginPath / "Contents";
    if (std::filesystem::exists(contentsDir)) {
        // Try to find the actual plugin binary
        std::vector<std::string> archDirs = {"x86_64-win", "win64", "Windows"};
        
        for (const auto& archDir : archDirs) {
            auto binaryPath = contentsDir / archDir;
            if (std::filesystem::exists(binaryPath)) {
                // VST3 binary should have same name as bundle but with .vst3 extension
                auto binaryFile = binaryPath / (info.name + ".vst3");
                if (std::filesystem::exists(binaryFile)) {
                    std::cout << "  Binary found: " << binaryFile << "\n";
                    break;
                }
            }
        }
    }
    
    // For now, assume plugin is valid if it has proper structure
    info.manufacturer = "Unknown";
    info.version = "Unknown";
    info.uid = "Unknown";
    
    return Result<VST3PluginInfo>::success(info);
}

std::string RealVST3Scanner::extractPluginName(const std::filesystem::path& pluginPath) {
    std::string filename = pluginPath.filename().string();
    // Remove .vst3 extension
    if (filename.length() > 5 && filename.substr(filename.length() - 5) == ".vst3") {
        return filename.substr(0, filename.length() - 5);
    }
    return filename;
}

void RealVST3Scanner::printDownloadInstructions() {
    std::cout << "\n=== VST3 Plugin Download Instructions ===\n";
    std::cout << "\nTo test real VST3 integration, please install one of these free plugins:\n\n";
    
    std::cout << "1. Voxengo Span (Spectrum Analyzer):\n";
    std::cout << "   Download: https://www.voxengo.com/product/span/\n";
    std::cout << "   Install to: C:/Program Files/Common Files/VST3/Span.vst3\n\n";
    
    std::cout << "2. TDR Nova (Dynamic EQ):\n";
    std::cout << "   Download: https://www.tokyodawn.net/tdr-nova/\n";
    std::cout << "   Install to: C:/Program Files/Common Files/VST3/TDR Nova.vst3\n\n";
    
    std::cout << "Alternative free VST3 plugins:\n";
    std::cout << "- ReaPlugs VST FX Suite (from Cockos)\n";
    std::cout << "- Melda Free Bundle (MeldaProduction)\n";
    std::cout << "- Blue Cat's Freeware Bundle\n\n";
    
    std::cout << "After installation, run: MixMindAI.exe --scan-vst3\n";
    std::cout << "==========================================\n\n";
}

} // namespace mixmind