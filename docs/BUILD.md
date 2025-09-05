# MixMind AI Build Guide

## Overview

MixMind AI supports two build configurations:

1. **Minimal Build** (`MIXMIND_MINIMAL=ON`) - Lightweight, CI-friendly
2. **Full Build** (`MIXMIND_MINIMAL=OFF`) - Complete DAW with audio dependencies

## Build Flags Audit

### Core Options (CMakeLists.txt:41-43)
```cmake
option(RUBBERBAND_ENABLED "Enable Rubber Band time stretching (commercial)" OFF)
option(BUILD_TESTS "Build unit tests" ON)
option(MIXMIND_MINIMAL "Build minimal, CI-friendly targets (no heavy audio deps)" OFF)
```

### MIXMIND_MINIMAL Impact

#### Dependencies Skipped (Minimal Build)
- **Lines 57-87**: Tracktion Engine + JUCE (heavy ~500MB download)
- **Lines 90-109**: nlohmann_json, httplib 
- **Lines 240-271**: All audio library linking

#### Conditional Builds
| Component | Minimal | Full | File Reference |
|-----------|---------|------|----------------|
| Tracktion/JUCE | ❌ | ✅ | CMakeLists.txt:57-87 |
| JSON/HTTP libs | ❌ | ✅ | CMakeLists.txt:90-109 |
| MixMind App | ❌ | ✅ | apps/mixmind_app/CMakeLists.txt:4 |
| Audio Services | ❌ | ✅ | src/services/CMakeLists.txt:2 |
| VST/Tracktion Adapters | ❌ | ✅ | src/adapters/CMakeLists.txt:2 |
| AI Services | ❌ | ✅ | src/ai/CMakeLists.txt:36 |

## Quick Start

### Minimal Build (CI/Testing)
```bash
cmake -S . -B build -DMIXMIND_MINIMAL=ON
cmake --build build --config Release
./build/Release/MixMindAI.exe  # Stub executable
```

**Use Cases:**
- Continuous Integration (all workflows use this)
- Quick syntax/structure validation  
- Unit testing without audio dependencies

### Full Build (Development)
```bash
cmake -S . -B build -DMIXMIND_MINIMAL=OFF
cmake --build build --config Release
./build/Release/mixmind_app.exe  # Complete DAW application
```

**Features Enabled:**
- Complete JUCE audio application
- Tracktion Engine integration
- Transport controls (Play/Stop/Loop/Record)
- VST3 plugin hosting
- Real-time audio processing
- Session save/load

## Build Verification

### Minimal Build Success
```bash
# Should build in ~30 seconds
cmake -S . -B build_minimal -DMIXMIND_MINIMAL=ON
cmake --build build_minimal --config Release
./build_minimal/Release/MixMindAI.exe
```

Expected output:
```
MixMind AI - Minimal Build
Build: MIXMIND_MINIMAL=ON
Core systems initialized
Exiting...
```

### Full Build Success  
```bash
# First build takes 5-10 minutes (downloads Tracktion)
cmake -S . -B build_full -DMIXMIND_MINIMAL=OFF
cmake --build build_full --config Release
./build_full/Release/mixmind_app.exe
```

Expected: JUCE window opens with transport controls

## Troubleshooting

### Full Build Issues
- **Long configure time**: Tracktion download is ~500MB
- **Build failures**: Ensure Visual Studio 2019+ on Windows
- **Missing audio devices**: Check JUCE device manager settings

### Minimal Build Issues
- **Missing main_minimal.cpp**: Should exist at `src/main_minimal.cpp`
- **Test failures**: Run with `ctest --output-on-failure`

## CI Integration

All GitHub Actions workflows use `MIXMIND_MINIMAL=ON`:
- **ci-windows.yml**: Windows build verification
- **ci-matrix.yml**: Cross-platform testing  
- **codeql-analysis.yml**: Security analysis
- **quality-gate.yml**: Coverage and metrics

This keeps CI fast (~2-3 minutes) while preserving full functionality for local development.