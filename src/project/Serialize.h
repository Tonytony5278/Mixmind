#pragma once
#include "Project.h"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace mixmind::project {

  // JSON serialization for project data
  class Serialize {
  public:
    // Convert project to JSON
    static nlohmann::json toJson(const Project& project);
    
    // Convert JSON to project
    static Project fromJson(const nlohmann::json& json);
    
    // Crash-safe save: write to *.tmp, fsync, atomically rename
    static bool saveToFile(const Project& project, const std::filesystem::path& filePath);
    
    // Load project from file
    static std::optional<Project> loadFromFile(const std::filesystem::path& filePath);
    
    // Schema migration (stub for future versions)
    static nlohmann::json migrate(const nlohmann::json& json, int fromVersion, int toVersion);
    
    // Validate JSON structure
    static bool validateJson(const nlohmann::json& json);
    
  private:
    // Helper methods for JSON conversion
    static nlohmann::json midiNoteToJson(const MidiNote& note);
    static MidiNote midiNoteFromJson(const nlohmann::json& json);
    
    static nlohmann::json pluginToJson(const Plugin& plugin);
    static Plugin pluginFromJson(const nlohmann::json& json);
    
    static nlohmann::json trackToJson(const Track& track);
    static Track trackFromJson(const nlohmann::json& json);
    
    static nlohmann::json tempoEventToJson(const TempoEvent& event);
    static TempoEvent tempoEventFromJson(const nlohmann::json& json);
  };

}