#include "Serialize.h"
#include <fstream>
#include <iostream>

// For atomic file operations
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace mixmind::project {

nlohmann::json Serialize::toJson(const Project& project) {
  nlohmann::json json;
  
  // Metadata
  json["schemaVersion"] = project.schemaVersion;
  json["name"] = project.name;
  json["created"] = project.created;
  json["modified"] = project.modified;
  
  // Timeline settings
  json["ticksPerQuarter"] = project.ticksPerQuarter;
  
  // Tempo map
  json["tempoMap"] = nlohmann::json::array();
  for (const auto& tempo : project.tempoMap) {
    json["tempoMap"].push_back(tempoEventToJson(tempo));
  }
  
  // Tracks
  json["tracks"] = nlohmann::json::array();
  for (const auto& track : project.tracks) {
    json["tracks"].push_back(trackToJson(track));
  }
  
  return json;
}

Project Serialize::fromJson(const nlohmann::json& json) {
  Project project;
  
  // Metadata
  if (json.contains("schemaVersion")) project.schemaVersion = json["schemaVersion"];
  if (json.contains("name")) project.name = json["name"];
  if (json.contains("created")) project.created = json["created"];
  if (json.contains("modified")) project.modified = json["modified"];
  
  // Timeline settings
  if (json.contains("ticksPerQuarter")) project.ticksPerQuarter = json["ticksPerQuarter"];
  
  // Tempo map
  project.tempoMap.clear();
  if (json.contains("tempoMap") && json["tempoMap"].is_array()) {
    for (const auto& tempoJson : json["tempoMap"]) {
      project.tempoMap.push_back(tempoEventFromJson(tempoJson));
    }
  }
  
  // Tracks
  project.tracks.clear();
  if (json.contains("tracks") && json["tracks"].is_array()) {
    for (const auto& trackJson : json["tracks"]) {
      project.tracks.push_back(trackFromJson(trackJson));
    }
  }
  
  return project;
}

bool Serialize::saveToFile(const Project& project, const std::filesystem::path& filePath) {
  try {
    // Generate JSON
    auto json = toJson(project);
    std::string jsonString = json.dump(2);  // Pretty print with 2-space indent
    
    // Create parent directories
    std::filesystem::create_directories(filePath.parent_path());
    
    // Write to temporary file first (crash-safe)
    auto tempPath = filePath;
    tempPath += ".tmp";
    
    {
      std::ofstream file(tempPath, std::ios::binary);
      if (!file) return false;
      
      file << jsonString;
      file.flush();
      
      // Force write to disk (fsync equivalent)
#ifdef _WIN32
      FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file._M_file()))));
#else
      fsync(fileno(file.rdbuf()->_M_file()));
#endif
    }
    
    // Atomic rename (crash-safe)
    std::error_code ec;
    std::filesystem::rename(tempPath, filePath, ec);
    
    return !ec;
    
  } catch (const std::exception& e) {
    std::cerr << "Failed to save project: " << e.what() << "\n";
    return false;
  }
}

std::optional<Project> Serialize::loadFromFile(const std::filesystem::path& filePath) {
  try {
    if (!std::filesystem::exists(filePath)) {
      return std::nullopt;
    }
    
    std::ifstream file(filePath);
    if (!file) return std::nullopt;
    
    nlohmann::json json;
    file >> json;
    
    if (!validateJson(json)) {
      std::cerr << "Invalid project JSON structure\n";
      return std::nullopt;
    }
    
    // Handle schema migration if needed
    int fileVersion = json.value("schemaVersion", 1);
    if (fileVersion < 1) {
      json = migrate(json, fileVersion, 1);
    }
    
    return fromJson(json);
    
  } catch (const std::exception& e) {
    std::cerr << "Failed to load project: " << e.what() << "\n";
    return std::nullopt;
  }
}

nlohmann::json Serialize::migrate(const nlohmann::json& json, int fromVersion, int toVersion) {
  // Stub for future schema migrations
  // For now, just upgrade version number
  auto migrated = json;
  migrated["schemaVersion"] = toVersion;
  return migrated;
}

bool Serialize::validateJson(const nlohmann::json& json) {
  // Basic structure validation
  if (!json.is_object()) return false;
  if (!json.contains("schemaVersion")) return false;
  if (json["schemaVersion"] < 1) return false;
  
  // Tracks should be array if present
  if (json.contains("tracks") && !json["tracks"].is_array()) return false;
  
  // Tempo map should be array if present  
  if (json.contains("tempoMap") && !json["tempoMap"].is_array()) return false;
  
  return true;
}

// Helper implementations
nlohmann::json Serialize::midiNoteToJson(const MidiNote& note) {
  return {
    {"startTick", note.startTick},
    {"duration", note.duration},
    {"pitch", note.pitch},
    {"velocity", note.velocity}
  };
}

MidiNote Serialize::midiNoteFromJson(const nlohmann::json& json) {
  return MidiNote(
    json.value("startTick", 0),
    json.value("duration", 480),
    json.value("pitch", 60),
    json.value("velocity", 80)
  );
}

nlohmann::json Serialize::pluginToJson(const Plugin& plugin) {
  nlohmann::json json;
  json["id"] = plugin.id;
  json["preset"] = plugin.preset;
  json["parameters"] = plugin.parameters;
  return json;
}

Plugin Serialize::pluginFromJson(const nlohmann::json& json) {
  Plugin plugin;
  plugin.id = json.value("id", "");
  plugin.preset = json.value("preset", "");
  
  if (json.contains("parameters") && json["parameters"].is_object()) {
    plugin.parameters = json["parameters"];
  }
  
  return plugin;
}

nlohmann::json Serialize::trackToJson(const Track& track) {
  nlohmann::json json;
  json["name"] = track.name;
  json["color"] = track.color;
  json["muted"] = track.muted;
  json["soloed"] = track.soloed;
  json["volume"] = track.volume;
  json["pan"] = track.pan;
  
  // Plugins
  json["plugins"] = nlohmann::json::array();
  for (const auto& plugin : track.plugins) {
    json["plugins"].push_back(pluginToJson(plugin));
  }
  
  // MIDI notes
  json["midiNotes"] = nlohmann::json::array();
  for (const auto& note : track.midiNotes) {
    json["midiNotes"].push_back(midiNoteToJson(note));
  }
  
  return json;
}

Track Serialize::trackFromJson(const nlohmann::json& json) {
  Track track;
  track.name = json.value("name", "Track");
  track.color = json.value("color", "#808080");
  track.muted = json.value("muted", false);
  track.soloed = json.value("soloed", false);
  track.volume = json.value("volume", 1.0f);
  track.pan = json.value("pan", 0.0f);
  
  // Plugins
  if (json.contains("plugins") && json["plugins"].is_array()) {
    for (const auto& pluginJson : json["plugins"]) {
      track.plugins.push_back(pluginFromJson(pluginJson));
    }
  }
  
  // MIDI notes
  if (json.contains("midiNotes") && json["midiNotes"].is_array()) {
    for (const auto& noteJson : json["midiNotes"]) {
      track.midiNotes.push_back(midiNoteFromJson(noteJson));
    }
  }
  
  return track;
}

nlohmann::json Serialize::tempoEventToJson(const TempoEvent& event) {
  return {
    {"tick", event.tick},
    {"bpm", event.bpm}
  };
}

TempoEvent Serialize::tempoEventFromJson(const nlohmann::json& json) {
  return TempoEvent(
    json.value("tick", 0),
    json.value("bpm", 120.0)
  );
}

}