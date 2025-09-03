@echo off
REM ===================================================================
REM MixMind AI Alpha - One-Click Professional Setup
REM ===================================================================

title MixMind AI Alpha Setup

echo.
echo =====================================================
echo       MixMind AI Alpha - One-Click Setup
echo =====================================================
echo.
echo This will build and launch MixMind AI Alpha
echo Professional DAW with AI-first interaction
echo.
echo Features included:
echo - VST3 Plugin Hosting with real plugin detection
echo - Piano Roll Editor with complete MIDI editing
echo - Real-time Automation System
echo - Professional Mixer with EBU R128 metering
echo - Multi-format Rendering Engine
echo - AI Assistant with natural language control
echo.
pause

echo.
echo [STEP 1/2] Building MixMind AI Alpha...
echo.
call Build_MixMind_Alpha.bat

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed. Please check the error messages above.
    pause
    exit /b 1
)

echo.
echo [STEP 2/2] Launching MixMind AI Alpha Test Suite...
echo.
call Launch_MixMind_Alpha.bat

echo.
echo Setup complete! MixMind AI Alpha is ready to use.
pause