#pragma once
#include <string>
#include <set>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace mixmind::plugins {
  
  // Simple JSON registry for crashed/timeout plugins
  class Quarantine {
  public:
    explicit Quarantine(const std::filesystem::path& cacheFile = "build-cache/plugin_quarantine.json");
    
    // Check if plugin is quarantined
    bool isBlocked(const std::string& pluginPath) const;
    
    // Add plugin to quarantine (after crash/timeout)
    void block(const std::string& pluginPath, const std::string& reason = "crash");
    
    // Remove plugin from quarantine (manual recovery)
    void unblock(const std::string& pluginPath);
    
    // Save quarantine state to disk
    void save();
    
    // Load quarantine state from disk
    void load();
    
    // Get all quarantined plugins
    const std::set<std::string>& getBlocked() const { return blocked_; }
    
  private:
    std::filesystem::path cacheFile_;
    std::set<std::string> blocked_;
    nlohmann::json metadata_;  // Reasons, timestamps, etc.
  };
  
}