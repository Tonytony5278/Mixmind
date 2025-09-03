# MixMind AI - Setup & Installation

This directory contains all installation and validation tools for MixMind AI Alpha.

## Quick Start

**One-Click Installation:**
```bash
# From project root directory
MixMind_AI_Professional_Setup.bat
```

## Directory Structure

```
setup/
├── scripts/           # Build and launch scripts
│   ├── Build_MixMind_Alpha.bat      # Professional build script
│   ├── Launch_MixMind_Alpha.bat     # Interactive test launcher  
│   └── MixMind_Alpha_Setup.bat      # Original Alpha setup
├── validation/        # Validation and testing scripts
│   ├── Validate_Alpha_Setup.py      # System validation
│   ├── python_test_piano_roll.py    # Piano Roll validation
│   ├── python_test_automation.py    # Automation validation
│   ├── python_test_mixer.py         # Mixer validation
│   ├── python_test_render.py        # Rendering validation
│   └── python_test_ai_assistant.py  # AI Assistant validation
└── README.md          # This file
```

## Installation Options

### 1. Quick Install (Recommended)
- Builds complete MixMind AI DAW
- Launches interactive test suite
- ~3-5 minutes total time

### 2. Custom Install
- Choose specific components
- Selective feature installation
- Advanced user option

### 3. Developer Install
- Full development environment
- Debug builds with symbols
- Performance profiling tools
- Desktop shortcuts for development

### 4. Validation Only
- System requirement checks
- Component validation
- No building or installation

## System Requirements

**Minimum:**
- Windows 10/11 (x64)
- CMake 3.22+
- Visual Studio 2019 Build Tools or newer
- 4GB RAM, 2GB disk space

**Recommended:**
- 8GB+ RAM for large projects
- SSD for optimal performance
- Audio interface for professional use

## What Gets Installed

**Core DAW Systems:**
- VST3 Plugin Hosting with real plugin detection
- Piano Roll MIDI Editor with complete note manipulation
- Real-time Automation System with sub-10ms latency
- Professional Mixer with EBU R128 broadcast metering
- Multi-format Rendering (WAV, AIFF, FLAC) with LUFS
- AI Assistant with natural language control

**Development Tools:**
- Complete test suite (75+ comprehensive tests)
- Validation scripts for all systems
- Build system with dependency management
- Performance benchmarking tools

## Validation Scripts

Each major system has dedicated validation:

```bash
python validation/python_test_piano_roll.py     # MIDI system
python validation/python_test_automation.py     # Automation
python validation/python_test_mixer.py          # Audio processing
python validation/python_test_render.py         # Export system
python validation/python_test_ai_assistant.py   # AI features
```

## Troubleshooting

**Build Fails:**
- Ensure CMake 3.22+ is installed
- Check Visual Studio Build Tools are present
- Run validation script to identify issues

**Tests Fail:**
- VST3 tests require VST plugins to be installed
- Audio tests may need audio drivers
- AI tests require network connectivity for language models

**Performance Issues:**
- Use Release build configuration
- Ensure adequate RAM (8GB+ recommended)
- Close unnecessary applications during testing

## Support

- **Documentation:** ../README.md
- **GitHub:** https://github.com/Tonytony5278/Mixmind
- **Issues:** Report bugs via GitHub Issues