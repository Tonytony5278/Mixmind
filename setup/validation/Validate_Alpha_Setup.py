#!/usr/bin/env python3
"""
MixMind AI Alpha - Setup Validation Script
"""

import os
import sys
import subprocess

def main():
    print("=" * 60)
    print("    MixMind AI Alpha - Setup Validation")
    print("=" * 60)
    print()
    
    # Check if we're in the right directory
    expected_files = [
        "MixMind_Alpha_Setup.bat",
        "Build_MixMind_Alpha.bat", 
        "Launch_MixMind_Alpha.bat",
        "CMakeLists.txt",
        "README.md"
    ]
    
    print("[VALIDATION] Checking required files...")
    missing_files = []
    for file in expected_files:
        if os.path.exists(file):
            print(f"  [OK] {file}")
        else:
            print(f"  [MISSING] {file}")
            missing_files.append(file)
    
    if missing_files:
        print(f"\n[ERROR] Missing files: {missing_files}")
        return False
    
    # Check Alpha implementation files
    print("\n[VALIDATION] Checking Alpha implementation...")
    alpha_dirs = [
        "src/vsti",     # VST3 Plugin Hosting
        "src/midi",     # Piano Roll Editor  
        "src/automation", # Automation System
        "src/mixer",    # Professional Mixer
        "src/render",   # Rendering Engine
        "src/ai",       # AI Assistant
        "tests"         # Test Coverage
    ]
    
    for directory in alpha_dirs:
        if os.path.exists(directory):
            file_count = len([f for f in os.listdir(directory) if f.endswith(('.h', '.cpp'))])
            print(f"  [OK] {directory} - {file_count} files")
        else:
            print(f"  [MISSING] {directory}")
    
    # Check Python validation scripts
    print("\n[VALIDATION] Checking validation scripts...")
    validation_scripts = [
        "python_test_piano_roll.py",
        "python_test_automation.py", 
        "python_test_mixer.py",
        "python_test_render.py",
        "python_test_ai_assistant.py"
    ]
    
    for script in validation_scripts:
        if os.path.exists(script):
            print(f"  [OK] {script}")
        else:
            print(f"  [MISSING] {script}")
    
    # Check if CMake is available
    print("\n[VALIDATION] Checking build requirements...")
    cmake_paths = [
        r"C:\Program Files\CMake\bin\cmake.exe",
        r"C:\Program Files (x86)\CMake\bin\cmake.exe"
    ]
    
    cmake_found = False
    for path in cmake_paths:
        if os.path.exists(path):
            print(f"  [OK] CMake found at: {path}")
            cmake_found = True
            break
    
    if not cmake_found:
        print("  [WARNING] CMake not found. Please install CMake 3.22+")
        print("           Download from: https://cmake.org/download/")
    
    print("\n" + "=" * 60)
    
    if missing_files or not cmake_found:
        print("    Alpha Setup: NEEDS ATTENTION")
        if missing_files:
            print(f"    Missing files: {len(missing_files)}")
        if not cmake_found:
            print("    CMake installation required")
    else:
        print("    Alpha Setup: READY FOR TESTING")
        print("    Run: MixMind_Alpha_Setup.bat")
    
    print("=" * 60)
    print()
    
    return len(missing_files) == 0

if __name__ == "__main__":
    success = main()
    if success:
        print("✅ MixMind AI Alpha setup validation passed!")
    else:
        print("❌ MixMind AI Alpha setup validation failed!")
        sys.exit(1)