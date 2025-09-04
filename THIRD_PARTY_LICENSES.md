# Third-Party Licenses

MixMind AI incorporates several open-source and commercial third-party libraries. This document provides a comprehensive overview of all dependencies and their licensing requirements.

## Core Dependencies (Always Present)

### Catch2 Testing Framework
- **Version**: v3.4.0
- **License**: BSL-1.0 (Boost Software License 1.0)
- **Purpose**: Unit testing and benchmarking framework
- **Website**: https://github.com/catchorg/Catch2
- **License Text**: https://github.com/catchorg/Catch2/blob/devel/LICENSE.txt

### nlohmann/json
- **Version**: v3.11.3
- **License**: MIT License
- **Purpose**: JSON parsing and serialization
- **Website**: https://github.com/nlohmann/json
- **License Text**: https://github.com/nlohmann/json/blob/develop/LICENSE.MIT

### cpp-httplib
- **Version**: v0.15.3
- **License**: MIT License
- **Purpose**: HTTP client for AI API integration
- **Website**: https://github.com/yhirose/cpp-httplib
- **License Text**: https://github.com/yhirose/cpp-httplib/blob/master/LICENSE

## Audio Dependencies (Full Build Only)

### Tracktion Engine
- **Version**: develop branch
- **License**: Dual-licensed (GPL v3 / Commercial)
- **Purpose**: Professional DAW engine and audio processing
- **Website**: https://github.com/Tracktion/tracktion_engine
- **License Options**:
  - **GPL v3**: Free for open-source projects
  - **Commercial**: Required for commercial distribution
- **License Text**: https://github.com/Tracktion/tracktion_engine/blob/develop/LICENSE

**⚠️ Important**: Tracktion Engine requires either GPL v3 compliance (making your entire application GPL v3) or a commercial license for proprietary use.

### JUCE Framework (via Tracktion Engine)
- **Version**: Bundled with Tracktion Engine
- **License**: Dual-licensed (GPL v3 / Commercial)  
- **Purpose**: Cross-platform audio framework and GUI toolkit
- **Website**: https://juce.com/
- **License Options**:
  - **GPL v3**: Free for open-source projects
  - **Commercial**: JUCE Personal ($40/month) or JUCE Pro ($130/month)
- **License Text**: https://github.com/juce-framework/JUCE/blob/master/LICENSE.md

### VST3 SDK
- **Version**: master branch
- **License**: Steinberg VST3 License
- **Purpose**: VST3 plugin hosting and development
- **Website**: https://github.com/steinbergmedia/vst3sdk
- **License Text**: https://github.com/steinbergmedia/vst3sdk/blob/master/LICENSE.txt

**Note**: The VST3 SDK allows free use for VST3 plugin hosting. Commercial plugin development may require additional agreements.

## Optional Dependencies

### SoundTouch (Currently Disabled)
- **Version**: 2.3.2
- **License**: LGPL v2.1
- **Purpose**: Time-stretching and pitch-shifting algorithms
- **Website**: https://gitlab.com/soundtouch/soundtouch
- **Status**: Commented out in CMakeLists.txt for PoC phase

### Rubber Band Library (Optional)
- **Version**: Commercial version required
- **License**: Commercial License Required
- **Purpose**: Professional time-stretching and pitch-shifting
- **Website**: https://breakfastquay.com/rubberband/
- **Status**: Opt-in via `RUBBERBAND_ENABLED=ON`

## Build Tools and CI Dependencies

### CMake
- **Minimum Version**: 3.22
- **License**: BSD 3-Clause License
- **Purpose**: Build system and dependency management
- **Website**: https://cmake.org/

### GitHub Actions (CI/CD)
- **Service**: GitHub's continuous integration platform
- **License**: Per GitHub Terms of Service
- **Purpose**: Automated testing and deployment

## Licensing Compatibility Matrix

| Component | License | Compatible with MIT Core | Notes |
|-----------|---------|---------------------------|--------|
| MixMind AI Core | MIT | ✅ Base License | Our code |
| Catch2 | BSL-1.0 | ✅ Compatible | Testing only |
| nlohmann/json | MIT | ✅ Compatible | Always included |
| cpp-httplib | MIT | ✅ Compatible | Optional HTTP |
| Tracktion Engine | GPL v3 / Commercial | ⚠️ Requires Decision | See below |
| JUCE | GPL v3 / Commercial | ⚠️ Requires Decision | Via Tracktion |
| VST3 SDK | Steinberg License | ✅ Compatible | Plugin hosting OK |
| SoundTouch | LGPL v2.1 | ⚠️ Complex | Currently disabled |
| Rubber Band | Commercial | ⚠️ Commercial Only | Optional |

## Licensing Decision Required: Tracktion Engine & JUCE

MixMind AI's use of Tracktion Engine and JUCE creates a licensing decision point:

### Option 1: GPL v3 Compliance
- **Requirement**: Entire MixMind AI must be licensed under GPL v3
- **Impact**: Users can freely use, modify, and distribute
- **Limitation**: Commercial proprietary use prohibited
- **Cost**: Free

### Option 2: Commercial Licensing
- **Requirement**: Purchase commercial licenses for Tracktion Engine and JUCE
- **Impact**: Can maintain MIT license for MixMind AI core
- **Benefit**: Commercial use and proprietary distribution allowed
- **Cost**: JUCE Personal ($40/month) + Tracktion commercial license

### Option 3: Minimal Build Mode
- **Approach**: Exclude Tracktion Engine and JUCE from binary distributions
- **Impact**: Users build full version locally with their own license choices
- **Benefit**: Core MIT licensing preserved
- **Limitation**: Users must handle audio dependencies themselves

## Current Implementation Strategy

MixMind AI currently implements **Option 3 (Dual Build Mode)**:

1. **Minimal Build** (`MIXMIND_MINIMAL=ON`):
   - MIT licensed core functionality
   - No GPL or commercial dependencies
   - Suitable for CI/CD and development
   - Can be distributed as MIT-licensed binaries

2. **Full Build** (`MIXMIND_MINIMAL=OFF`):
   - Includes all audio dependencies
   - Requires user to accept dependency licenses
   - Not suitable for binary redistribution without license compliance
   - Users build locally with their chosen licensing

## Developer Responsibilities

### For Contributors
- All contributions to MixMind AI core remain MIT licensed
- Contributors should not introduce dependencies with incompatible licenses
- New dependencies must be added to this document

### For Users Building Full Version
- Accept Tracktion Engine license terms (GPL v3 or commercial)
- Accept JUCE license terms (GPL v3 or commercial)
- Accept VST3 SDK license terms
- Ensure compliance if redistributing binaries

### For Commercial Users
- Evaluate licensing requirements for your use case
- Consider commercial licenses for Tracktion Engine and JUCE
- Consult legal counsel for complex licensing scenarios

## Compliance Notes

### GPL v3 Copyleft Requirements
If using Tracktion Engine under GPL v3:
- Entire combined work must be GPL v3 licensed
- Source code must be provided to recipients
- Modifications must be shared under GPL v3
- Commercial distribution requires source availability

### Commercial License Benefits
With commercial licenses:
- No copyleft requirements
- Proprietary use and distribution allowed
- No source code disclosure required
- Commercial support available

## License Texts

Full license texts for all components are available in their respective repositories or can be found in the `licenses/` directory of this project.

## Questions and Support

For licensing questions:
- **General**: Open an issue on GitHub
- **Commercial Licensing**: Contact component vendors directly:
  - JUCE: https://juce.com/get-juce
  - Tracktion Engine: https://www.tracktion.com/develop/tracktion-engine
- **Legal Advice**: Consult your legal counsel

## Updates

This document is updated as dependencies change. Last updated: January 2025.

---

**Disclaimer**: This document provides general information about third-party licenses but is not legal advice. Consult qualified legal counsel for licensing compliance in your specific situation.