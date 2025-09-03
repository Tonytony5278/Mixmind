#include "Quarantine.h"
#include <fstream>
#include <chrono>
#include <iostream>

namespace mixmind::plugins {

Quarantine::Quarantine(const std::filesystem::path& cacheFile) 
  : cacheFile_(cacheFile) {
  load();
}

bool Quarantine::isBlocked(const std::string& pluginPath) const {
  return blocked_.find(pluginPath) != blocked_.end();
}

void Quarantine::block(const std::string& pluginPath, const std::string& reason) {
  blocked_.insert(pluginPath);
  
  // Add metadata
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  
  metadata_[pluginPath] = {
    {"reason", reason},
    {"timestamp", timestamp},
    {"blocked", true}
  };
  
  save();
  std::cerr << "Quarantined plugin: " << pluginPath << " (" << reason << ")\n";
}

void Quarantine::unblock(const std::string& pluginPath) {
  blocked_.erase(pluginPath);
  
  if (metadata_.contains(pluginPath)) {
    metadata_[pluginPath]["blocked"] = false;
  }
  
  save();
  std::cerr << "Unquarantined plugin: " << pluginPath << "\n";
}

void Quarantine::save() {
  try {
    // Ensure directory exists
    std::filesystem::create_directories(cacheFile_.parent_path());
    
    nlohmann::json output;
    output["version"] = 1;
    output["blocked_plugins"] = nlohmann::json::array();
    
    for (const auto& path : blocked_) {
      output["blocked_plugins"].push_back(path);
    }
    
    output["metadata"] = metadata_;
    
    std::ofstream file(cacheFile_);
    file << output.dump(2);
    
  } catch (const std::exception& e) {
    std::cerr << "Failed to save quarantine cache: " << e.what() << "\n";
  }
}

void Quarantine::load() {
  try {
    if (!std::filesystem::exists(cacheFile_)) {
      return;  // No cache file yet
    }
    
    std::ifstream file(cacheFile_);
    nlohmann::json data;
    file >> data;
    
    blocked_.clear();
    
    if (data.contains("blocked_plugins") && data["blocked_plugins"].is_array()) {
      for (const auto& path : data["blocked_plugins"]) {
        if (path.is_string()) {
          blocked_.insert(path.get<std::string>());
        }
      }
    }
    
    if (data.contains("metadata") && data["metadata"].is_object()) {
      metadata_ = data["metadata"];
    }
    
  } catch (const std::exception& e) {
    std::cerr << "Failed to load quarantine cache: " << e.what() << "\n";
    blocked_.clear();
    metadata_.clear();
  }
}

}