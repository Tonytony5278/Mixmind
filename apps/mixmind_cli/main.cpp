#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

// Silent WAV writer (deterministic for testing)
static void write_silent_wav(const std::string& path, int sr, int seconds) {
  struct WavHeader { 
    char RIFF[4]; uint32_t size; char WAVE[4]; 
    char fmt_[4]; uint32_t fmtSize; uint16_t audioFmt; uint16_t ch; 
    uint32_t srate; uint32_t brate; uint16_t block; uint16_t bits; 
    char data[4]; uint32_t dsize; 
  } h{
    {'R','I','F','F'}, 0, {'W','A','V','E'}, 
    {'f','m','t',' '}, 16, 1, 2, 
    (uint32_t)sr, (uint32_t)(sr*2*2), 4, 16, 
    {'d','a','t','a'}, 0
  };
  
  std::vector<uint8_t> data(sr * seconds * h.block, 0);
  h.dsize = (uint32_t)data.size();
  h.size  = 36 + h.dsize;
  
  FILE* f = std::fopen(path.c_str(), "wb"); 
  if (!f) {
    std::cerr << "Error: Could not write to " << path << std::endl;
    return;
  }
  
  std::fwrite(&h, sizeof(h), 1, f);
  std::fwrite(data.data(), data.size(), 1, f);
  std::fclose(f);
}

// Parse command line arguments
struct Args {
  std::string project = "";
  std::string output = "out.wav";
  int sampleRate = 48000;
  int seconds = 3;
  bool showVersion = false;
  bool showHelp = false;
};

Args parse_args(int argc, char** argv) {
  Args args;
  
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    
    if (arg == "--version") {
      args.showVersion = true;
    } else if (arg == "--help" || arg == "-h") {
      args.showHelp = true;
    } else if (arg == "--project" && i + 1 < argc) {
      args.project = argv[++i];
    } else if (arg == "--out" && i + 1 < argc) {
      args.output = argv[++i];
    } else if (arg == "--sr" && i + 1 < argc) {
      args.sampleRate = std::stoi(argv[++i]);
    } else if (arg == "--seconds" && i + 1 < argc) {
      args.seconds = std::stoi(argv[++i]);
    }
  }
  
  return args;
}

void show_help() {
  std::cout << R"(MixMind CLI - Headless Audio Renderer

Usage:
  mixmind_cli [options]

Options:
  --version           Show version information
  --help, -h          Show this help message
  --project FILE      Load project JSON file (future)
  --out FILE          Output WAV file (default: out.wav)
  --sr RATE           Sample rate (default: 48000)
  --seconds N         Duration in seconds (default: 3)

Examples:
  mixmind_cli --version
  mixmind_cli --out test.wav --sr 44100 --seconds 5
  mixmind_cli --project song.json --out song.wav

Note: This is currently a silent stub renderer for CI testing.
Full engine integration will enable real project rendering.
)";
}

int main(int argc, char** argv) {
  Args args = parse_args(argc, argv);
  
  if (args.showVersion) { 
    std::cout << "mixmind_cli 0.1.0\n"; 
    return 0; 
  }
  
  if (args.showHelp) {
    show_help();
    return 0;
  }
  
  // Validate arguments
  if (args.sampleRate <= 0 || args.sampleRate > 192000) {
    std::cerr << "Error: Invalid sample rate " << args.sampleRate << std::endl;
    return 1;
  }
  
  if (args.seconds <= 0 || args.seconds > 3600) {
    std::cerr << "Error: Invalid duration " << args.seconds << " seconds" << std::endl;
    return 1;
  }
  
  // Check if project file exists (for future use)
  if (!args.project.empty()) {
    if (!std::filesystem::exists(args.project)) {
      std::cerr << "Error: Project file not found: " << args.project << std::endl;
      return 1;
    }
    std::cout << "Loading project: " << args.project << " (stub - not implemented yet)" << std::endl;
  }
  
  std::cout << "Rendering " << args.seconds << "s at " << args.sampleRate << "Hz..." << std::endl;
  
  // TODO: If full engine available: load project JSON, render to WAV (offline)
  // For now: write deterministic silent WAV for testing
  write_silent_wav(args.output, args.sampleRate, args.seconds);
  
  std::cout << "Rendered " << args.output << " (silent stub)" << std::endl;
  return 0;
}