@echo off
REM ===================================================================
REM MixMind AI Alpha - Professional Launcher
REM ===================================================================

echo.
echo ==================================================
echo         MixMind AI Alpha - Test Suite
echo ==================================================
echo.

set BUILD_DIR=build\Release

REM Check if build exists
if not exist "%BUILD_DIR%" (
    echo [ERROR] Build not found. Please run Build_MixMind_Alpha.bat first
    pause
    exit /b 1
)

:MENU
echo.
echo Select Alpha component to test:
echo.
echo [1] VST3 Plugin Hosting    - Real plugin detection and hosting
echo [2] Piano Roll Editor      - Complete MIDI editing system
echo [3] Automation System      - Real-time parameter automation
echo [4] Professional Mixer     - EBU R128 metering and bus routing
echo [5] Rendering Engine       - Multi-format audio export
echo [6] AI Assistant           - Natural language DAW control
echo [7] Core Systems Test      - Foundation components
echo [8] Run All Tests          - Complete Alpha validation
echo [9] Exit
echo.
set /p choice="Enter your choice (1-9): "

if "%choice%"=="1" goto VST_TEST
if "%choice%"=="2" goto PIANO_TEST
if "%choice%"=="3" goto AUTO_TEST
if "%choice%"=="4" goto MIXER_TEST
if "%choice%"=="5" goto RENDER_TEST
if "%choice%"=="6" goto AI_TEST
if "%choice%"=="7" goto CORE_TEST
if "%choice%"=="8" goto ALL_TESTS
if "%choice%"=="9" goto EXIT
goto MENU

:VST_TEST
echo.
echo ==================================================
echo    Testing VST3 Plugin Hosting System
echo ==================================================
if exist "%BUILD_DIR%\test_vsti.exe" (
    "%BUILD_DIR%\test_vsti.exe"
) else (
    echo [ERROR] test_vsti.exe not found. Please build first.
)
pause
goto MENU

:PIANO_TEST
echo.
echo ==================================================
echo    Testing Piano Roll Editor System
echo ==================================================
if exist "%BUILD_DIR%\test_piano_roll.exe" (
    "%BUILD_DIR%\test_piano_roll.exe"
) else (
    echo [ERROR] test_piano_roll.exe not found. Please build first.
)
pause
goto MENU

:AUTO_TEST
echo.
echo ==================================================
echo    Testing Automation System
echo ==================================================
if exist "%BUILD_DIR%\test_automation.exe" (
    "%BUILD_DIR%\test_automation.exe"
) else (
    echo [ERROR] test_automation.exe not found. Please build first.
)
pause
goto MENU

:MIXER_TEST
echo.
echo ==================================================
echo    Testing Professional Mixer System
echo ==================================================
if exist "%BUILD_DIR%\test_mixer.exe" (
    "%BUILD_DIR%\test_mixer.exe"
) else (
    echo [ERROR] test_mixer.exe not found. Please build first.
)
pause
goto MENU

:RENDER_TEST
echo.
echo ==================================================
echo    Testing Rendering Engine
echo ==================================================
if exist "%BUILD_DIR%\test_render.exe" (
    "%BUILD_DIR%\test_render.exe"
) else (
    echo [ERROR] test_render.exe not found. Please build first.
)
pause
goto MENU

:AI_TEST
echo.
echo ==================================================
echo    Testing AI Assistant System
echo ==================================================
if exist "%BUILD_DIR%\test_ai_assistant.exe" (
    "%BUILD_DIR%\test_ai_assistant.exe"
) else (
    echo [ERROR] test_ai_assistant.exe not found. Please build first.
)
pause
goto MENU

:CORE_TEST
echo.
echo ==================================================
echo    Testing Core Systems
echo ==================================================
if exist "%BUILD_DIR%\core_test.exe" (
    "%BUILD_DIR%\core_test.exe"
) else (
    echo [ERROR] core_test.exe not found. Please build first.
)
pause
goto MENU

:ALL_TESTS
echo.
echo ==================================================
echo    Running Complete Alpha Validation
echo ==================================================
echo.
echo Running Python validation scripts...
python setup\validation\python_test_piano_roll.py
python setup\validation\python_test_automation.py
python setup\validation\python_test_mixer.py
python setup\validation\python_test_render.py
python setup\validation\python_test_ai_assistant.py
echo.
echo ==================================================
echo    Alpha Validation Complete
echo ==================================================
pause
goto MENU

:EXIT
echo.
echo Thank you for testing MixMind AI Alpha!
echo.
pause
exit