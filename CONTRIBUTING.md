# Contributing to MixMind AI

Thank you for your interest in contributing to MixMind AI! This document provides guidelines for contributing to this AI-first Digital Audio Workstation.

## 🎯 Project Overview

MixMind AI is a professional DAW with AI-first interaction, implementing "Cursor meets Logic Pro" with natural language control and comprehensive music production capabilities.

**Current Status**: Alpha Complete (380K+ lines, 75+ tests)

## 🚀 Getting Started

### Prerequisites

**Required:**
- CMake 3.22+
- C++20 compatible compiler (MSVC 2022, GCC 10+, Clang 12+)
- Git with LFS support

**Recommended:**
- Visual Studio 2022 (Windows)
- VST3 SDK for plugin development
- Audio interface for testing

### Quick Setup

1. **Clone and Build**
```bash
git clone --recursive https://github.com/Tonytony5278/Mixmind.git
cd Mixmind

# Windows (Visual Studio)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Linux/macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

2. **Run Tests**
```bash
cd build
ctest -C Release --output-on-failure
```

3. **Validate Setup**
```bash
python setup/validation/Validate_Alpha_Setup.py
```

## 📋 How to Contribute

### Reporting Issues

**Bug Reports**: Use [bug_report.md](.github/ISSUE_TEMPLATE/bug_report.md) template
- Include system specs, audio setup, and reproduction steps
- Attach relevant logs and error messages

**Feature Requests**: Use [feature_request.md](.github/ISSUE_TEMPLATE/feature_request.md) template
- Describe use case and implementation considerations
- Consider real-time performance impact

### Development Workflow

1. **Fork and Branch**
```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/issue-description
```

2. **Make Changes**
- Follow existing code style and patterns
- Add comprehensive tests for new functionality
- Update documentation as needed

3. **Pre-commit Setup**
```bash
# Install pre-commit hooks (one-time setup)
pip install pre-commit
pre-commit install

# Manual run (optional - hooks run automatically on commit)
pre-commit run --all-files
```

4. **Test Thoroughly**
```bash
# Run all tests
cmake --build build --target test

# Validate specific systems
python setup/validation/python_test_*.py

# Test real-time performance
./build/Release/test_automation.exe  # etc.
```

5. **Submit Pull Request**
- Use the PR template checklist
- Ensure all CI passes (cross-platform matrix + quality gates)
- Pre-commit hooks must pass
- Request review from @Tonytony5278

## 🏗️ Architecture Overview

```
src/
├── core/           # Foundation types and interfaces
├── vsti/           # VST3 plugin hosting
├── midi/           # Piano Roll and MIDI processing  
├── automation/     # Real-time automation engine
├── mixer/          # Professional audio mixing
├── render/         # Multi-format audio export
├── ai/             # AI Assistant and intelligence
├── audio/          # Audio processing and metering
└── ui/             # User interface components

tests/              # Comprehensive test suite
setup/              # Installation and validation tools
```

## 🎵 Development Guidelines

### Code Style

**C++20 Modern Patterns:**
```cpp
// Use Result<T> for error handling
core::Result<AudioBuffer> processAudio(const AudioBuffer& input);

// RAII and smart pointers
auto processor = std::make_unique<AudioProcessor>();

// Structured bindings
auto [success, result] = validateInput(data);
```

**Real-time Audio Safety:**
- No dynamic allocation in audio callbacks
- Use lock-free data structures for real-time threads
- Target sub-10ms latency for critical paths
- Test with various buffer sizes (64-2048 samples)

**Error Handling:**
- Use monadic `Result<T>` pattern throughout
- Provide meaningful error messages
- Handle edge cases gracefully

### Testing Standards

**Required for All PRs:**
- Unit tests for new functionality
- Integration tests for system interactions
- Real-time performance validation
- Memory leak checks

**Audio Testing:**
```cpp
TEST(AutomationEngine, RealTimeProcessing) {
    AutomationEngine engine;
    AudioBuffer buffer(512);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = engine.processBuffer(buffer);
    auto duration = std::chrono::high_resolution_clock::now() - start;
    
    EXPECT_TRUE(result.is_ok());
    EXPECT_LT(duration.count(), 10000000); // < 10ms
}
```

### Performance Requirements

**Real-time Audio:**
- Maximum 10ms processing latency
- No blocking operations in audio thread
- Efficient memory usage patterns

**Plugin Integration:**
- Sample-accurate VST parameter automation
- Proper plugin delay compensation
- Stable plugin lifecycle management

**AI Integration:**
- Responsive natural language processing
- Context-aware command recognition
- Efficient audio analysis algorithms

## 🔧 Specific Areas

### Audio Engine Contributions
- Focus on real-time safety and low latency
- Test with multiple audio interfaces and sample rates
- Consider ASIO, WASAPI, CoreAudio compatibility

### AI System Contributions  
- Improve natural language command recognition
- Enhance context awareness and learning
- Add new creative AI features

### Plugin System Contributions
- Expand VST3 compatibility and features
- Add AU (macOS) and LV2 (Linux) support
- Improve plugin scanning and management

### UI/UX Contributions
- Modern, responsive interface design
- Accessibility improvements
- Cross-platform consistency

## 📝 Pull Request Process

1. **Pre-submission Checklist:**
   - [ ] All tests pass locally
   - [ ] Code follows project style
   - [ ] Documentation updated
   - [ ] Performance benchmarks acceptable

2. **PR Requirements:**
   - Clear description of changes
   - Link to related issues
   - Screenshots for UI changes
   - Performance impact assessment

3. **Review Process:**
   - Automated CI checks must pass
   - Code review by maintainer
   - Testing on multiple configurations
   - Final approval and merge

## 🎼 Audio Development Notes

### Working with Real-time Audio
- Use `AudioBuffer` class for all audio data
- Respect thread boundaries (UI vs audio thread)
- Test with realistic audio loads and plugin chains

### VST Plugin Development
- Follow VST3 SDK best practices
- Implement proper state serialization
- Handle plugin parameter changes smoothly

### MIDI Processing
- Support all standard MIDI messages
- Handle timing accurately (sample-level precision)
- Test with various MIDI controllers and devices

## 🤝 Community

### Getting Help
- **GitHub Discussions**: General questions and ideas
- **Issues**: Bug reports and feature requests  
- **Code Review**: Learn from existing implementations

### Code of Conduct
- Be respectful and constructive
- Focus on technical merit
- Help newcomers learn the codebase
- Maintain professional communication

## 📜 License

By contributing to MixMind AI, you agree that your contributions will be licensed under the MIT License.

**Important**: We currently use some components with different licenses (Tracktion Engine, JUCE). Please be aware of licensing implications for your contributions.

---

**Thank you for contributing to the future of music production!** 🎵

For questions about contributing, feel free to open a discussion or reach out to @Tonytony5278.