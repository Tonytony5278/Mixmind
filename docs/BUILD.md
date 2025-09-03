# Building MixMind AI

This guide provides exact commands for building MixMind AI on all supported platforms.

## Prerequisites

### All Platforms
- **CMake**: 3.22 or later
- **Git**: With submodule support
- **C++20 Compiler**: See platform-specific requirements below

### Windows
- **Visual Studio 2022** (Build Tools or Community Edition)
  - Workloads: "C++ build tools" with Windows 10/11 SDK
- **Alternative**: Visual Studio 2019 (minimum 16.11)

### macOS
- **Xcode 15** or later (for clang with C++20 support)
- **macOS**: 11.0 (Big Sur) or later
- **Apple Silicon**: Native builds supported

### Linux
- **Ubuntu 24.04** / **Fedora 39** / **Debian 12** (or equivalent)
- **GCC 14** or **Clang 18** (for complete C++20 support)
- **System packages**: See platform sections below

## Quick Start

### Windows (Visual Studio)

```powershell
# Clone repository
git clone --recursive https://github.com/Tonytony5278/Mixmind.git
cd Mixmind

# Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# Build  
cmake --build build --config Release --parallel 4

# Test
cd build
ctest -C Release --output-on-failure

# Run
./Release/mixmind.exe
```

### macOS (Xcode)

```bash
# Clone repository
git clone --recursive https://github.com/Tonytony5278/Mixmind.git
cd Mixmind

# Configure
cmake -S . -B build -G "Xcode" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

# Build
cmake --build build --config Release -- -parallelizeTargets -jobs 4

# Test
cd build
ctest -C Release --output-on-failure

# Run
open Release/MixMind.app
```

### Linux (Ninja)

```bash
# Install dependencies (Ubuntu 24.04)
sudo apt-get update
sudo apt-get install -y \
  build-essential ninja-build \
  gcc-14 g++-14 \
  libasound2-dev libjack-jackd2-dev \
  libgl1-mesa-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev

# Clone repository
git clone --recursive https://github.com/Tonytony5278/Mixmind.git
cd Mixmind

# Configure
export CC=gcc-14 CXX=g++-14
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel 4

# Test
cd build
ctest --output-on-failure --parallel 4

# Run
./mixmind
```

## Build Options

### Common CMake Options

```bash
# Build type
-DCMAKE_BUILD_TYPE=Release          # Optimized build (recommended)
-DCMAKE_BUILD_TYPE=Debug            # Debug symbols, slower
-DCMAKE_BUILD_TYPE=RelWithDebInfo   # Optimized + debug symbols

# Dependency options
-DTRACKTION_BUILD_EXAMPLES=OFF      # Skip Tracktion examples (recommended)
-DTRACKTION_BUILD_TESTS=OFF         # Skip Tracktion tests (recommended)
-DRUBBERBAND_ENABLED=ON             # Enable time-stretching (requires license)

# Install prefix
-DCMAKE_INSTALL_PREFIX=/usr/local   # Installation directory
```

### Platform-Specific Options

**Windows:**
```powershell
-G "Visual Studio 17 2022" -A x64   # Use VS 2022, 64-bit
-DCPACK_GENERATOR=NSIS              # Enable NSIS installer packaging
```

**macOS:**
```bash
-G "Xcode"                          # Use Xcode generator
-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0  # Minimum macOS version
-DCPACK_GENERATOR=DragNDrop         # Enable DMG packaging
```

**Linux:**
```bash
-G "Ninja"                          # Fast ninja builds
-DCPACK_GENERATOR="TGZ;DEB;RPM"     # Multiple package formats
```

## Troubleshooting

### Common Issues

**"CMake version too old"**
```bash
# Ubuntu/Debian
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt update && sudo apt install cmake

# macOS
brew install cmake

# Windows
# Download from https://cmake.org/download/
```

**"C++20 features not supported"**
- Windows: Update to Visual Studio 2022
- macOS: Update to Xcode 15+
- Linux: Use GCC 14+ or Clang 18+

**"Submodule errors"**
```bash
git submodule update --init --recursive
# Or clone with: git clone --recursive
```

**"VST3 SDK not found"**
```bash
# The VST3 SDK should be fetched automatically
# If issues persist, manually clone:
git clone https://github.com/steinbergmedia/vst3sdk.git external/vst3sdk
```

### Audio Driver Issues

**Windows (ASIO)**
- Install ASIO4ALL for universal ASIO support
- Or use audio interface manufacturer's ASIO driver

**macOS (Core Audio)**  
- Core Audio is built-in, no additional drivers needed
- Check Audio MIDI Setup for proper device configuration

**Linux (ALSA/JACK)**
```bash
# ALSA (recommended for basic use)
sudo apt-get install alsa-utils

# JACK (recommended for professional use)  
sudo apt-get install jackd2 qjackctl
```

## Development Build

For development with faster incremental builds:

```bash
# Configure with debug info and faster builds
cmake -S . -B build -G "Ninja" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# Install ccache for faster rebuilds
# Ubuntu: sudo apt install ccache
# macOS: brew install ccache  
# Windows: Use sccache instead
```

## Testing

### Run All Tests
```bash
cd build
ctest --output-on-failure --parallel 4
```

### Run Specific Test Categories
```bash
# Core system tests
ctest -R "core" --output-on-failure

# Audio processing tests  
ctest -R "audio|vsti|mixer" --output-on-failure

# AI system tests
ctest -R "ai" --output-on-failure
```

### Performance Tests
```bash
# Run with timing information
ctest --output-on-failure --verbose

# Run performance benchmarks
./build/Release/test_performance
```

## Installation

### System Installation
```bash
# Install to system directories
cmake --install build --config Release

# Create packages
cd build
cpack -C Release
```

### Portable Installation
```bash
# Copy binaries to portable directory
mkdir MixMind-Portable
cp build/Release/mixmind* MixMind-Portable/
cp -r assets/ MixMind-Portable/
```

## Validation

After building, validate your installation:

```bash
# Run the validation script
python setup/validation/Validate_Alpha_Setup.py

# Or use the professional installer
./MixMind_Professional_Installer.bat  # Windows
./MixMind_Professional_Installer.sh   # Linux/macOS (if available)
```

## CI/CD Builds

The project includes comprehensive CI/CD that builds on:
- **Windows**: Visual Studio 2022 (MSVC)
- **macOS**: Xcode 15 (Clang)  
- **Linux**: GCC 14 and Clang 18

See `.github/workflows/ci-matrix.yml` for the exact CI configuration.

---

**Need Help?** Open an issue at https://github.com/Tonytony5278/Mixmind/issues