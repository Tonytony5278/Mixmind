#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <filesystem>

namespace mixmind::project {

  // Minimal MIDI note representation
  struct MidiNote {
    int startTick;
    int duration;
    int pitch;      // 0-127
    int velocity;   // 0-127
    
    MidiNote() = default;
    MidiNote(int start, int dur, int p, int vel) 
      : startTick(start), duration(dur), pitch(p), velocity(vel) {}
  };

  // Plugin instance on a track
  struct Plugin {
    std::string id;         // "Reverb", "Compressor", etc.
    std::string preset;     // Optional preset name
    std::map<std::string, float> parameters;  // Parameter automation
    
    Plugin() = default;
    Plugin(const std::string& pluginId, const std::string& presetName = "")
      : id(pluginId), preset(presetName) {}
  };

  // Audio/MIDI track
  struct Track {
    std::string name;
    std::string color;      // "#FF5733" hex format
    bool muted = false;
    bool soloed = false;
    float volume = 1.0f;    // Linear gain
    float pan = 0.0f;       // -1.0 to 1.0
    
    std::vector<Plugin> plugins;
    std::vector<MidiNote> midiNotes;
    
    // Audio clips (future extension)
    // std::vector<AudioClip> audioClips;
  };

  // Tempo change event
  struct TempoEvent {
    int tick;
    double bpm;
    
    TempoEvent() = default;
    TempoEvent(int t, double b) : tick(t), bpm(b) {}
  };

  // Main project container
  class Project {
  public:
    // Metadata
    int schemaVersion = 1;
    std::string name = "Untitled Project";
    std::string created;    // ISO timestamp
    std::string modified;   // ISO timestamp
    
    // Timeline settings
    int ticksPerQuarter = 480;
    std::vector<TempoEvent> tempoMap;
    
    // Content
    std::vector<Track> tracks;
    
    // Methods
    Project();
    
    // Track management
    void addTrack(const std::string& name, const std::string& color = "#808080");
    Track* getTrack(size_t index);
    const Track* getTrack(size_t index) const;
    size_t getTrackCount() const { return tracks.size(); }
    
    // Plugin management
    void insertPlugin(size_t trackIndex, const std::string& pluginId, const std::string& preset = "");
    
    // MIDI note management  
    void addMidiNote(size_t trackIndex, int startTick, int duration, int pitch, int velocity);
    
    // Tempo management
    void setTempo(double bpm, int tick = 0);
    double getCurrentTempo() const;
    
    // Validation
    bool validate() const;
    
  private:
    void updateModifiedTime();
  };

}