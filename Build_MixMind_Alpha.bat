@echo off
REM ===================================================================
REM MixMind AI Alpha - Professional Build Script
REM ===================================================================

echo.
echo ==================================================
echo    MixMind AI Alpha - Professional Build
echo ==================================================
echo.

REM Set build directory
set BUILD_DIR=build
set CMAKE_EXE="C:\Program Files\CMake\bin\cmake.exe"

REM Check if CMake exists
if not exist %CMAKE_EXE% (
    echo [ERROR] CMake not found at %CMAKE_EXE%
    echo Please install CMake 3.22+ from https://cmake.org/download/
    pause
    exit /b 1
)

REM Clean previous build
if exist %BUILD_DIR% (
    echo [INFO] Cleaning previous build...
    rmdir /s /q %BUILD_DIR%
)

echo [INFO] Configuring MixMind AI Alpha build...
%CMAKE_EXE% -S . -B %BUILD_DIR% -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    pause
    exit /b 1
)

echo [INFO] Building MixMind AI Alpha...
%CMAKE_EXE% --build %BUILD_DIR% --config Release --parallel 4

if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    pause
    exit /b 1
)

echo.
echo ==================================================
echo    MixMind AI Alpha - Build Complete!
echo ==================================================
echo.
echo Executables built in: %BUILD_DIR%\Release\
echo.
echo Available test programs:
if exist "%BUILD_DIR%\Release\test_vsti.exe" echo   - test_vsti.exe          [VST3 Plugin Hosting]
if exist "%BUILD_DIR%\Release\test_piano_roll.exe" echo   - test_piano_roll.exe    [Piano Roll Editor]
if exist "%BUILD_DIR%\Release\test_automation.exe" echo   - test_automation.exe    [Automation System]
if exist "%BUILD_DIR%\Release\test_mixer.exe" echo   - test_mixer.exe         [Professional Mixer]
if exist "%BUILD_DIR%\Release\test_render.exe" echo   - test_render.exe        [Rendering Engine]
if exist "%BUILD_DIR%\Release\test_ai_assistant.exe" echo   - test_ai_assistant.exe  [AI Assistant]
if exist "%BUILD_DIR%\Release\core_test.exe" echo   - core_test.exe          [Core Systems]

echo.
echo To launch MixMind AI Alpha, run:
echo   Launch_MixMind_Alpha.bat
echo.
pause