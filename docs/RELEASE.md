# Release Process

This document describes how MixMind AI's automated release process works and how to prepare releases.

## Overview

MixMind AI uses a fully automated CI/CD pipeline that:
1. **Triggers on tags**: Any tag matching `v*` pattern triggers release builds
2. **Multi-platform builds**: Automatically builds for Windows, macOS, and Linux
3. **Package generation**: Creates platform-specific installers and packages
4. **GitHub Releases**: Automatically creates and uploads release artifacts

## Release Workflow

### 1. Automated Triggers

The release process automatically triggers on:
```bash
git tag -a v1.0.0 -m "Release message"
git push origin v1.0.0
```

**Tag format**: `v<major>.<minor>.<patch>[-prerelease]`
- `v1.0.0` - Stable release  
- `v1.0.0-alpha.1` - Alpha release
- `v1.0.0-beta.2` - Beta release
- `v1.0.0-rc.1` - Release candidate

### 2. Build Matrix

Each release builds on:

| Platform | OS | Compiler | Output |
|----------|----|---------|---------| 
| Windows | windows-latest | MSVC 2022 | `MixMind-Setup-v1.0.0.exe` (NSIS) |
| macOS | macos-13 | Xcode 15 | `MixMind-v1.0.0.dmg` (DragNDrop) |
| Linux | ubuntu-24.04 | GCC 14 | `MixMind-v1.0.0.AppImage` & `.tar.gz` |

### 3. Packaging Details

**Windows (NSIS Installer)**
- Full installer with Start Menu shortcuts
- Uninstaller generation
- Registry integration
- Automatic dependency detection

**macOS (DMG)**  
- Drag-and-drop installer
- Application bundle (`MixMind.app`)
- TODO: Code signing and notarization (requires Apple Developer account)

**Linux (AppImage + tar.gz)**
- Portable AppImage for universal Linux compatibility
- Traditional tar.gz archive for package managers
- Desktop integration files included

### 4. Artifacts Generated

Each release includes:
- **Platform Installers**: Ready-to-distribute packages
- **Debug Artifacts**: Symbols, logs, and test results for debugging
- **Release Notes**: Auto-generated from commit messages and tag annotations

## Creating a Release

### Step 1: Prepare Release

```bash
# Ensure you're on main and up to date
git checkout main
git pull origin main

# Update version in CMakeLists.txt if needed
# project(MixMindAI VERSION 1.0.0 LANGUAGES CXX C)

# Create comprehensive tag message
git tag -a v1.0.0 -m "MixMind AI v1.0.0

🎵 Production Release - Complete Professional DAW

## New Features
- Feature 1 description
- Feature 2 description

## Improvements  
- Improvement 1
- Improvement 2

## Bug Fixes
- Fix 1 description
- Fix 2 description

## Technical Details
- Platform support: Windows, macOS, Linux
- C++20 with modern audio processing
- VST3 plugin hosting with sub-10ms latency
- Professional broadcast compliance (EBU R128)

## Installation
Download the appropriate installer for your platform from the release assets.

---
**Full Changelog**: https://github.com/Tonytony5278/Mixmind/compare/v0.9.0...v1.0.0"
```

### Step 2: Push Release

```bash
# Push the tag to trigger automated release
git push origin v1.0.0

# Monitor release progress
# https://github.com/Tonytony5278/Mixmind/actions
```

### Step 3: Verify Release

After CI completes (typically 15-30 minutes):

1. **Check GitHub Release**: https://github.com/Tonytony5278/Mixmind/releases
2. **Verify Artifacts**: All platform packages should be present
3. **Test Downloads**: Ensure installers work on target platforms
4. **Update Documentation**: Update any version-specific documentation

## Release Checklist

### Pre-Release
- [ ] All CI checks passing on main branch
- [ ] Version number updated in `CMakeLists.txt` 
- [ ] `CHANGELOG.md` updated (if maintained)
- [ ] Documentation reflects new features
- [ ] Beta testing completed for major releases

### During Release
- [ ] Tag created with comprehensive message
- [ ] Tag pushed to trigger CI
- [ ] CI builds complete successfully
- [ ] All artifacts uploaded to GitHub Release

### Post-Release  
- [ ] Release announcement prepared
- [ ] Documentation website updated
- [ ] Package managers notified (if applicable)
- [ ] Community notified (Discord, forums, etc.)

## Manual Release Steps (If Needed)

If the automated release fails, you can manually create releases:

### Manual Build Commands

```bash
# Windows (PowerShell)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cd build && cpack -C Release -G NSIS

# macOS  
cmake -S . -B build -G "Xcode" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cd build && cpack -C Release -G DragNDrop

# Linux
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release  
cmake --build build
cd build && cpack -G TGZ
# AppImage creation would need additional setup
```

### Manual Release Upload

```bash
# Using GitHub CLI
gh release create v1.0.0 \
  --title "MixMind AI v1.0.0" \
  --notes "Release notes here" \
  build/MixMind-Setup-v1.0.0.exe \
  build/MixMind-v1.0.0.dmg \
  build/MixMind-v1.0.0.tar.gz
```

## Code Signing & Notarization

### Windows Code Signing (TODO)
```powershell
# Requires code signing certificate
signtool sign /f certificate.pfx /p password /t http://timestamp.digicert.com MixMind-Setup.exe
```

### macOS Code Signing & Notarization (TODO)
```bash
# Requires Apple Developer account and certificates
export APPLE_DEVELOPER_ID="Developer ID Application: Your Name"
export APPLE_DEVELOPER_PASSWORD="app-specific-password"
export APPLE_TEAM_ID="TEAM_ID"

# Sign the application
codesign --force --deep --sign "$APPLE_DEVELOPER_ID" MixMind.app

# Create signed DMG
create-dmg --volname "MixMind AI" --background background.png MixMind.dmg MixMind.app

# Submit for notarization
xcrun notarytool submit MixMind.dmg \
  --apple-id your@email.com \
  --password "$APPLE_DEVELOPER_PASSWORD" \
  --team-id "$APPLE_TEAM_ID" \
  --wait

# Staple notarization
xcrun stapler staple MixMind.dmg
```

## Release Security

### Artifact Integrity
- All release artifacts include SHA256 checksums
- Binaries are built in isolated CI environment
- Source code integrity verified via Git commit signatures

### Signing Status
- **Windows**: Code signing TODO (requires certificate)
- **macOS**: Code signing and notarization TODO (requires Apple Developer account)  
- **Linux**: GPG signing of packages TODO

### Security Considerations
- Release builds disable debug features
- Crash reporting and telemetry are opt-in only
- No sensitive data included in binaries
- Dependencies vetted for security vulnerabilities

## Hotfix Releases

For critical bug fixes:

```bash
# Create hotfix branch from release tag
git checkout -b hotfix/v1.0.1 v1.0.0

# Make minimal fix
# ... fix code ...

# Test thoroughly
# ... run tests ...

# Create hotfix tag
git tag -a v1.0.1 -m "Hotfix v1.0.1 - Critical bug fix"
git push origin v1.0.1

# Merge back to main
git checkout main
git merge hotfix/v1.0.1
git push origin main
```

## Monitoring

### Release Metrics
- Download counts (GitHub Insights)
- Platform distribution analysis  
- User feedback and issue reports
- Performance metrics from telemetry (if opt-in)

### Success Criteria
- All platform builds complete successfully
- Installers work on target platforms
- No critical bugs reported within 24 hours
- User adoption metrics positive

---

**Questions?** Contact the maintainers or open an issue for release process improvements.