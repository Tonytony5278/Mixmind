#pragma once

#include <nlohmann/json.hpp>

namespace mixmind::api::schemas {

using json = nlohmann::json;

// ============================================================================
// Common Parameter Schemas
// ============================================================================

/// Track ID parameter schema
const json TRACK_ID_SCHEMA = json{
    {"type", "object"},
    {"properties", {
        {"trackId", {
            {"type", "integer"},
            {"minimum", 1},
            {"description", "Unique track identifier"}
        }}
    }},
    {"required", {"trackId"}}
};

/// Clip ID parameter schema
const json CLIP_ID_SCHEMA = json{
    {"type", "object"},
    {"properties", {
        {"clipId", {
            {"type", "integer"},
            {"minimum", 1},
            {"description", "Unique clip identifier"}
        }}
    }},
    {"required", {"clipId"}}
};

/// Plugin ID parameter schema
const json PLUGIN_ID_SCHEMA = json{
    {"type", "object"},
    {"properties", {
        {"pluginId", {
            {"type", "integer"},
            {"minimum", 1},
            {"description", "Unique plugin identifier"}
        }}
    }},
    {"required", {"pluginId"}}
};

/// Time position parameter schema
const json TIME_POSITION_SCHEMA = json{
    {"type", "object"},
    {"properties", {
        {"position", {
            {"type", "number"},
            {"minimum", 0.0},
            {"description", "Time position in seconds"}
        }}
    }},
    {"required", {"position"}}
};

/// Volume parameter schema
const json VOLUME_SCHEMA = json{
    {"type", "object"},
    {"properties", {
        {"volume", {
            {"type", "number"},
            {"minimum", 0.0},
            {"maximum", 2.0},
            {"description", "Volume level (0.0 = silence, 1.0 = unity, 2.0 = +6dB)"}
        }}
    }},
    {"required", {"volume"}}
};

/// Pan parameter schema
const json PAN_SCHEMA = json{
    {"type", "object"},
    {"properties", {
        {"pan", {
            {"type", "number"},
            {"minimum", -1.0},
            {"maximum", 1.0},
            {"description", "Pan position (-1.0 = full left, 0.0 = center, 1.0 = full right)"}
        }}
    }},
    {"required", {"pan"}}
};

/// File path parameter schema
const json FILE_PATH_SCHEMA = json{
    {"type", "object"},
    {"properties", {
        {"filePath", {
            {"type", "string"},
            {"minLength", 1},
            {"description", "Absolute or relative file path"}
        }}
    }},
    {"required", {"filePath"}}
};

// ============================================================================
// Session Action Schemas
// ============================================================================

const json SESSION_ACTION_SCHEMAS = json{
    {"session.new", {
        {"type", "object"},
        {"properties", {
            {"name", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "Session name"}
            }},
            {"sampleRate", {
                {"type", "integer"},
                {"enum", {44100, 48000, 88200, 96000, 176400, 192000}},
                {"description", "Sample rate in Hz"}
            }},
            {"bitDepth", {
                {"type", "integer"},
                {"enum", {16, 24, 32}},
                {"description", "Bit depth"}
            }},
            {"template", {
                {"type", "string"},
                {"description", "Optional template to use"}
            }}
        }},
        {"required", {"name"}}
    }},
    
    {"session.open", {
        {"type", "object"},
        {"properties", {
            {"filePath", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "Path to session file"}
            }},
            {"readOnly", {
                {"type", "boolean"},
                {"description", "Open in read-only mode"}
            }}
        }},
        {"required", {"filePath"}}
    }},
    
    {"session.save", {
        {"type", "object"},
        {"properties", {
            {"filePath", {
                {"type", "string"},
                {"description", "Optional save path (defaults to current path)"}
            }},
            {"createBackup", {
                {"type", "boolean"},
                {"description", "Create backup before saving"}
            }}
        }}
    }},
    
    {"session.saveAs", {
        {"type", "object"},
        {"properties", {
            {"filePath", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "New save path"}
            }},
            {"createBackup", {
                {"type", "boolean"},
                {"description", "Create backup before saving"}
            }}
        }},
        {"required", {"filePath"}}
    }},
    
    {"session.close", {
        {"type", "object"},
        {"properties", {
            {"saveChanges", {
                {"type", "boolean"},
                {"description", "Save changes before closing"}
            }}
        }}
    }},
    
    {"session.export", {
        {"type", "object"},
        {"properties", {
            {"format", {
                {"type", "string"},
                {"enum", {"wav", "aiff", "flac", "mp3", "ogg"}},
                {"description", "Export format"}
            }},
            {"filePath", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "Export path"}
            }},
            {"sampleRate", {
                {"type", "integer"},
                {"enum", {44100, 48000, 88200, 96000, 176400, 192000}}
            }},
            {"bitDepth", {
                {"type", "integer"},
                {"enum", {16, 24, 32}}
            }},
            {"quality", {
                {"type", "integer"},
                {"minimum", 0,
                {"maximum", 10},
                {"description", "Quality level for lossy formats"}
            }}
        }},
        {"required", {"format", "filePath"}}
    }}
};

// ============================================================================
// Transport Action Schemas
// ============================================================================

const json TRANSPORT_ACTION_SCHEMAS = json{
    {"transport.play", {
        {"type", "object"},
        {"properties", {
            {"fromPosition", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Start position in seconds"}
            }}
        }}
    }},
    
    {"transport.stop", {
        {"type", "object"},
        {"properties", {}}
    }},
    
    {"transport.pause", {
        {"type", "object"},
        {"properties", {}}
    }},
    
    {"transport.record", {
        {"type", "object"},
        {"properties", {
            {"prepareOnly", {
                {"type", "boolean"},
                {"description", "Prepare for recording without starting"}
            }}
        }}
    }},
    
    {"transport.setPosition", {
        {"type", "object"},
        {"properties", {
            {"position", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Position in seconds"}
            }},
            {"snap", {
                {"type", "boolean"},
                {"description", "Snap to grid"}
            }}
        }},
        {"required", {"position"}}
    }},
    
    {"transport.setTempo", {
        {"type", "object"},
        {"properties", {
            {"tempo", {
                {"type", "number"},
                {"minimum", 20.0},
                {"maximum", 300.0},
                {"description", "Tempo in BPM"}
            }}
        }},
        {"required", {"tempo"}}
    }},
    
    {"transport.setLoop", {
        {"type", "object"},
        {"properties", {
            {"enabled", {
                {"type", "boolean"},
                {"description", "Enable/disable looping"}
            }},
            {"startPosition", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Loop start in seconds"}
            }},
            {"endPosition", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Loop end in seconds"}
            }}
        }},
        {"required", {"enabled"}}
    }}
};

// ============================================================================
// Track Action Schemas
// ============================================================================

const json TRACK_ACTION_SCHEMAS = json{
    {"track.create", {
        {"type", "object"},
        {"properties", {
            {"name", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "Track name"}
            }},
            {"type", {
                {"type", "string"},
                {"enum", {"audio", "midi", "instrument", "bus", "folder"}},
                {"description", "Track type"}
            }},
            {"color", {
                {"type", "string"},
                {"pattern", "^#[0-9A-Fa-f]{6}$"},
                {"description", "Track color in hex format"}
            }},
            {"position", {
                {"type", "integer"},
                {"minimum", 0},
                {"description", "Track position in list"}
            }}
        }},
        {"required", {"name", "type"}}
    }},
    
    {"track.delete", TRACK_ID_SCHEMA},
    
    {"track.setName", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"name", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "New track name"}
            }}
        }},
        {"required", {"trackId", "name"}}
    }},
    
    {"track.setVolume", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"volume", {
                {"type", "number"},
                {"minimum", 0.0},
                {"maximum", 2.0}
            }}
        }},
        {"required", {"trackId", "volume"}}
    }},
    
    {"track.setPan", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"pan", {
                {"type", "number"},
                {"minimum", -1.0},
                {"maximum", 1.0}
            }}
        }},
        {"required", {"trackId", "pan"}}
    }},
    
    {"track.setMute", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"muted", {
                {"type", "boolean"},
                {"description", "Mute state"}
            }}
        }},
        {"required", {"trackId", "muted"}}
    }},
    
    {"track.setSolo", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"solo", {
                {"type", "boolean"},
                {"description", "Solo state"}
            }}
        }},
        {"required", {"trackId", "solo"}}
    }},
    
    {"track.setRecordEnable", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"enabled", {
                {"type", "boolean"},
                {"description", "Record enable state"}
            }}
        }},
        {"required", {"trackId", "enabled"}}
    }}
};

// ============================================================================
// Clip Action Schemas
// ============================================================================

const json CLIP_ACTION_SCHEMAS = json{
    {"clip.create", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"startPosition", {
                {"type", "number"},
                {"minimum", 0.0}
            }},
            {"length", {
                {"type", "number"},
                {"minimum", 0.001}
            }},
            {"type", {
                {"type", "string"},
                {"enum", {"audio", "midi"}},
                {"description", "Clip type"}
            }},
            {"filePath", {
                {"type", "string"},
                {"description", "Audio file path for audio clips"}
            }},
            {"name", {
                {"type", "string"},
                {"description", "Clip name"}
            }}
        }},
        {"required", {"trackId", "startPosition", "length", "type"}}
    }},
    
    {"clip.delete", CLIP_ID_SCHEMA},
    
    {"clip.move", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"newPosition", {
                {"type", "number"},
                {"minimum", 0.0}
            }},
            {"newTrackId", {
                {"type", "integer"},
                {"minimum", 1},
                {"description", "Optional new track ID for moving between tracks"}
            }},
            {"snap", {
                {"type", "boolean"},
                {"description", "Snap to grid"}
            }}
        }},
        {"required", {"clipId", "newPosition"}}
    }},
    
    {"clip.resize", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"newLength", {
                {"type", "number"},
                {"minimum", 0.001}
            }},
            {"newStartOffset", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "New start offset within source material"}
            }},
            {"preservePitch", {
                {"type", "boolean"},
                {"description", "Preserve pitch when time-stretching"}
            }}
        }},
        {"required", {"clipId", "newLength"}}
    }},
    
    {"clip.split", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"position", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Split position relative to clip start"}
            }}
        }},
        {"required", {"clipId", "position"}}
    }},
    
    {"clip.duplicate", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"count", {
                {"type", "integer"},
                {"minimum", 1,
                {"maximum", 100},
                {"description", "Number of duplicates"}
            }},
            {"spacing", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Time spacing between duplicates"}
            }}
        }},
        {"required", {"clipId"}}
    }}
};

// ============================================================================
// Plugin Action Schemas
// ============================================================================

const json PLUGIN_ACTION_SCHEMAS = json{
    {"plugin.load", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"pluginName", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "Plugin name or ID"}
            }},
            {"slotIndex", {
                {"type", "integer"},
                {"minimum", 0},
                {"description", "Plugin slot index on track"}
            }},
            {"preset", {
                {"type", "string"},
                {"description", "Optional preset to load"}
            }}
        }},
        {"required", {"trackId", "pluginName"}}
    }},
    
    {"plugin.unload", PLUGIN_ID_SCHEMA},
    
    {"plugin.setParameter", {
        {"type", "object"},
        {"properties", {
            {"pluginId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"parameterId", {
                {"type", "integer"},
                {"minimum", 0}
            }},
            {"value", {
                {"type", "number"},
                {"minimum", 0.0},
                {"maximum", 1.0},
                {"description", "Normalized parameter value (0.0 to 1.0)"}
            }},
            {"parameterName", {
                {"type", "string"},
                {"description", "Parameter name (alternative to parameterId)"}
            }}
        }},
        {"required", {"pluginId", "value"}},
        {"anyOf", [
            {"required", ["parameterId"]},
            {"required", ["parameterName"]}
        ]}
    }},
    
    {"plugin.bypass", {
        {"type", "object"},
        {"properties", {
            {"pluginId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"bypassed", {
                {"type", "boolean"},
                {"description", "Bypass state"}
            }}
        }},
        {"required", {"pluginId", "bypassed"}}
    }},
    
    {"plugin.loadPreset", {
        {"type", "object"},
        {"properties", {
            {"pluginId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"presetName", {
                {"type", "string"},
                {"minLength", 1}
            }},
            {"presetPath", {
                {"type", "string"},
                {"description", "Path to preset file"}
            }}
        }},
        {"required", {"pluginId"}},
        {"anyOf": [
            {"required": ["presetName"]},
            {"required": ["presetPath"]}
        ]}
    }}
};

// ============================================================================
// Automation Action Schemas
// ============================================================================

const json AUTOMATION_ACTION_SCHEMAS = json{
    {"automation.createLane", {
        {"type", "object"},
        {"properties", {
            {"targetId", {
                {"type", "integer"},
                {"minimum", 1},
                {"description", "Track or plugin ID"}
            }},
            {"parameterId", {
                {"type", "integer"},
                {"minimum", 0}
            }},
            {"parameterName", {
                {"type", "string"},
                {"description", "Parameter name"}
            }},
            {"type", {
                {"type", "string"},
                {"enum", {"track", "plugin"}},
                {"description", "Target type"}
            }}
        }},
        {"required", {"targetId", "type"}},
        {"anyOf": [
            {"required": ["parameterId"]},
            {"required": ["parameterName"]}
        ]}
    }},
    
    {"automation.addPoint", {
        {"type", "object"},
        {"properties", {
            {"automationId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"time", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Time position in seconds"}
            }},
            {"value", {
                {"type", "number"},
                {"minimum", 0.0},
                {"maximum", 1.0},
                {"description", "Normalized parameter value"}
            }},
            {"curveType", {
                {"type", "string"},
                {"enum", {"linear", "exponential", "logarithmic", "smooth"}},
                {"description", "Curve type to next point"}
            }}
        }},
        {"required", {"automationId", "time", "value"}}
    }},
    
    {"automation.removePoint", {
        {"type", "object"},
        {"properties", {
            {"automationId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"pointId", {
                {"type", "integer"},
                {"minimum", 1}
            }}
        }},
        {"required", {"automationId", "pointId"}}
    }},
    
    {"automation.clear", {
        {"type", "object"},
        {"properties", {
            {"automationId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"startTime", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Clear range start (optional)"}
            }},
            {"endTime", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Clear range end (optional)"}
            }}
        }},
        {"required", {"automationId"}}
    }}
};

// ============================================================================
// Render Action Schemas
// ============================================================================

const json RENDER_ACTION_SCHEMAS = json{
    {"render.bounceTrack", {
        {"type", "object"},
        {"properties", {
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"outputPath", {
                {"type", "string"},
                {"minLength", 1}
            }},
            {"format", {
                {"type", "string"},
                {"enum", {"wav", "aiff", "flac"}},
                {"description", "Output format"}
            }},
            {"sampleRate", {
                {"type", "integer"},
                {"enum", {44100, 48000, 88200, 96000, 176400, 192000}}
            }},
            {"bitDepth", {
                {"type", "integer"},
                {"enum", {16, 24, 32}}
            }},
            {"startTime", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Render start time"}
            }},
            {"endTime", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Render end time"}
            }},
            {"includeEffects", {
                {"type", "boolean"},
                {"description", "Include track effects in render"}
            }}
        }},
        {"required", {"trackId", "outputPath", "format"}}
    }},
    
    {"render.mixdown", {
        {"type", "object"},
        {"properties", {
            {"outputPath", {
                {"type", "string"},
                {"minLength", 1}
            }},
            {"format", {
                {"type", "string"},
                {"enum", {"wav", "aiff", "flac", "mp3", "ogg"}}
            }},
            {"sampleRate", {
                {"type", "integer"},
                {"enum", {44100, 48000, 88200, 96000, 176400, 192000}}
            }},
            {"bitDepth", {
                {"type", "integer"},
                {"enum", {16, 24, 32}}
            }},
            {"quality", {
                {"type", "integer"},
                {"minimum", 0},
                {"maximum", 10},
                {"description", "Quality for lossy formats"}
            }},
            {"stems", {
                {"type", "boolean"},
                {"description", "Render individual track stems"}
            }},
            {"normalize", {
                {"type", "boolean"},
                {"description", "Normalize output"}
            }}
        }},
        {"required", {"outputPath", "format"}}
    }}
};

// ============================================================================
// Media Library Action Schemas
// ============================================================================

const json MEDIA_LIBRARY_ACTION_SCHEMAS = json{
    {"media.scan", {
        {"type", "object"},
        {"properties", {
            {"directory", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "Directory to scan"}
            }},
            {"recursive", {
                {"type", "boolean"},
                {"description", "Scan subdirectories"}
            }},
            {"fileTypes", {
                {"type", "array"},
                {"items": {
                    {"type", "string"}
                },
                {"description", "File extensions to scan for"}
            }}
        }},
        {"required", {"directory"}}
    }},
    
    {"media.import", {
        {"type", "object"},
        {"properties", {
            {"filePath", {
                {"type", "string"},
                {"minLength", 1}
            }},
            {"trackId", {
                {"type", "integer"},
                {"minimum", 1},
                {"description", "Target track for import"}
            }},
            {"position", {
                {"type", "number"},
                {"minimum", 0.0},
                {"description", "Import position in seconds"}
            }},
            {"analyzeAudio", {
                {"type", "boolean"},
                {"description", "Analyze audio properties"}
            }}
        }},
        {"required", {"filePath"}}
    }},
    
    {"media.search", {
        {"type", "object"},
        {"properties", {
            {"query", {
                {"type", "string"},
                {"minLength", 1},
                {"description", "Search query"}
            }},
            {"filters", {
                {"type", "object"},
                {"properties", {
                    {"fileType", {"type", "string"}},
                    {"duration", {"type": "object"}},
                    {"sampleRate", {"type", "integer"}},
                    {"tags", {"type", "array", "items": {"type": "string"}}}
                }}
            }},
            {"maxResults", {
                {"type", "integer"},
                {"minimum", 1},
                {"maximum", 1000}
            }}
        }},
        {"required", {"query"}}
    }}
};

// ============================================================================
// Audio Processing Action Schemas
// ============================================================================

const json AUDIO_PROCESSING_ACTION_SCHEMAS = json{
    {"audio.normalize", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"targetLevel", {
                {"type", "number"},
                {"minimum", -60.0},
                {"maximum", 0.0},
                {"description", "Target level in dB"}
            }},
            {"mode", {
                {"type", "string"},
                {"enum", {"peak", "rms", "lufs"},
                {"description", "Normalization mode"}
            }}
        }},
        {"required", {"clipId", "targetLevel"}}
    }},
    
    {"audio.fadeIn", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"duration", {
                {"type", "number"},
                {"minimum", 0.001},
                {"description", "Fade duration in seconds"}
            }},
            {"curve", {
                {"type", "string"},
                {"enum", {"linear", "exponential", "logarithmic", "smooth"}},
                {"description", "Fade curve shape"}
            }}
        }},
        {"required", {"clipId", "duration"}}
    }},
    
    {"audio.fadeOut", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"duration", {
                {"type", "number"},
                {"minimum", 0.001}
            }},
            {"curve", {
                {"type", "string"},
                {"enum", {"linear", "exponential", "logarithmic", "smooth"}}
            }}
        }},
        {"required", {"clipId", "duration"}}
    }},
    
    {"audio.reverse", CLIP_ID_SCHEMA},
    
    {"audio.pitchShift", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"semitones", {
                {"type", "number"},
                {"minimum", -24.0},
                {"maximum", 24.0},
                {"description", "Pitch shift in semitones"}
            }},
            {"preserveFormants", {
                {"type", "boolean"},
                {"description", "Preserve formants for vocal content"}
            }}
        }},
        {"required", {"clipId", "semitones"}}
    }}
};

// ============================================================================
// OSS Service Action Schemas
// ============================================================================

const json OSS_SERVICE_ACTION_SCHEMAS = json{
    {"analysis.spectrum", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"fftSize", {
                {"type", "integer"},
                {"enum", {512, 1024, 2048, 4096, 8192}},
                {"description", "FFT size"}
            }},
            {"windowType", {
                {"type", "string"},
                {"enum", {"hanning", "hamming", "blackman", "rectangular"}},
                {"description", "Window function"}
            }}
        }},
        {"required", {"clipId"}}
    }},
    
    {"analysis.lufs", {
        {"type", "object"},
        {"properties", {
            {"clipId", {
                {"type", "integer"},
                {"minimum", 1}
            }},
            {"mode", {
                {"type", "string"},
                {"enum", {"integrated", "short-term", "momentary"}},
                {"description", "LUFS measurement mode"}
            }}
        }},
        {"required", {"clipId"}}
    }},
    
    {"metadata.read", FILE_PATH_SCHEMA},
    
    {"metadata.write", {
        {"type", "object"},
        {"properties", {
            {"filePath", {
                {"type", "string"},
                {"minLength", 1}
            }},
            {"metadata", {
                {"type", "object"},
                {"properties", {
                    {"title", {"type", "string"}},
                    {"artist", {"type", "string"}},
                    {"album", {"type", "string"}},
                    {"year", {"type", "integer"}},
                    {"genre", {"type", "string"}},
                    {"comment", {"type", "string"}}
                }}
            }}
        }},
        {"required", {"filePath", "metadata"}}
    }}
};

} // namespace mixmind::api::schemas