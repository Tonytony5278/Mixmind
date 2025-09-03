#include "Project.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace mixmind::project {

Project::Project() {
  // Set creation timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  created = modified = ss.str();
  
  // Default tempo
  setTempo(120.0);
}

void Project::addTrack(const std::string& name, const std::string& color) {
  Track track;
  track.name = name;
  track.color = color;
  tracks.push_back(track);
  updateModifiedTime();
}

Track* Project::getTrack(size_t index) {
  if (index >= tracks.size()) return nullptr;
  return &tracks[index];
}

const Track* Project::getTrack(size_t index) const {
  if (index >= tracks.size()) return nullptr;
  return &tracks[index];
}

void Project::insertPlugin(size_t trackIndex, const std::string& pluginId, const std::string& preset) {
  if (auto* track = getTrack(trackIndex)) {
    track->plugins.emplace_back(pluginId, preset);
    updateModifiedTime();
  }
}

void Project::addMidiNote(size_t trackIndex, int startTick, int duration, int pitch, int velocity) {
  if (auto* track = getTrack(trackIndex)) {
    // Clamp values to valid MIDI ranges
    pitch = std::clamp(pitch, 0, 127);
    velocity = std::clamp(velocity, 1, 127);  // 0 velocity = note off
    
    track->midiNotes.emplace_back(startTick, duration, pitch, velocity);
    updateModifiedTime();
  }
}

void Project::setTempo(double bpm, int tick) {
  // Remove existing tempo event at this tick
  tempoMap.erase(
    std::remove_if(tempoMap.begin(), tempoMap.end(),
      [tick](const TempoEvent& event) { return event.tick == tick; }),
    tempoMap.end());
  
  // Add new tempo event
  tempoMap.emplace_back(tick, bpm);
  
  // Keep tempo map sorted by tick
  std::sort(tempoMap.begin(), tempoMap.end(),
    [](const TempoEvent& a, const TempoEvent& b) { return a.tick < b.tick; });
  
  updateModifiedTime();
}

double Project::getCurrentTempo() const {
  if (tempoMap.empty()) return 120.0;  // Default BPM
  return tempoMap.back().bpm;  // Latest tempo (assumes sorted)
}

bool Project::validate() const {
  // Schema version check
  if (schemaVersion < 1) return false;
  
  // Tempo map validation
  for (const auto& tempo : tempoMap) {
    if (tempo.bpm <= 0 || tempo.bpm > 999.0) return false;
    if (tempo.tick < 0) return false;
  }
  
  // Track validation
  for (const auto& track : tracks) {
    // MIDI note validation
    for (const auto& note : track.midiNotes) {
      if (note.pitch < 0 || note.pitch > 127) return false;
      if (note.velocity < 1 || note.velocity > 127) return false;
      if (note.startTick < 0 || note.duration <= 0) return false;
    }
    
    // Parameter validation
    for (const auto& plugin : track.plugins) {
      for (const auto& [key, value] : plugin.parameters) {
        if (value < 0.0f || value > 1.0f) return false;  // Normalized 0-1
      }
    }
  }
  
  return true;
}

void Project::updateModifiedTime() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  modified = ss.str();
}

}