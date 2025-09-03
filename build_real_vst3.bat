@echo off
echo =============================================================
echo MixMind AI - Real VST3 Integration Build
echo =============================================================

set CMAKE_PATH="C:\Program Files\CMake\bin\cmake.exe"
set BUILD_DIR=build_real

echo Checking CMake installation...
%CMAKE_PATH% --version
if %errorlevel% neq 0 (
    echo ERROR: CMake not found at %CMAKE_PATH%
    echo Please install CMake from: https://cmake.org/download/
    exit /b 1
)

echo.
echo Cleaning previous build...
if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%

echo.
echo Configuring build with real VST3 integration...
%CMAKE_PATH% -S . -B %BUILD_DIR% ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DRUBBERBAND_ENABLED=ON ^
    -DVST3SDK_GIT_TAG=master ^
    -DTRACKTION_BUILD_EXAMPLES=OFF ^
    -DTRACKTION_BUILD_TESTS=OFF ^
    -DTRACKTION_ENABLE_WEBVIEW=OFF ^
    -DTRACKTION_ENABLE_ABLETON_LINK=OFF ^
    -DBUILD_TESTS=ON

if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo.
echo Building MixMind AI with real VST3 support...
%CMAKE_PATH% --build %BUILD_DIR% --config Release -j

if %errorlevel% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo =============================================================
echo Build completed successfully!
echo Executable: %BUILD_DIR%\Release\MixMindAI.exe
echo Running VST3 detection test...
echo =============================================================

cd %BUILD_DIR%\Release
MixMindAI.exe --scan-vst3