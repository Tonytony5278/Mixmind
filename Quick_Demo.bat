@echo off
REM ===================================================================
REM MixMind AI - Quick Demo Launcher (For Impatient Users!)
REM ===================================================================

title MixMind AI - Quick Demo

echo.
echo ===================================================
echo    MixMind AI - Quick Access
echo ===================================================
echo.
echo Choose your experience:
echo.
echo [1] 🚀 INSTANT DEMO     - See features now (no waiting!)
echo [2] 💼 PROFESSIONAL     - Full installation (30 seconds)
echo [3] 🔧 DEVELOPER        - Advanced setup and testing
echo [4] ❌ Cancel
echo.
set /p choice="What do you want? (1-4): "

if "%choice%"=="1" (
    echo.
    echo 🚀 Launching instant demo...
    call MixMind_Alpha_Demo.bat
) else if "%choice%"=="2" (
    echo.
    echo 💼 Launching professional installer...
    call MixMind_Professional_Installer.bat
) else if "%choice%"=="3" (
    echo.
    echo 🔧 Launching developer setup...
    if exist "MixMind_AI_Professional_Setup.bat" (
        call MixMind_AI_Professional_Setup.bat
    ) else (
        echo Advanced setup not available. Using professional installer instead.
        call MixMind_Professional_Installer.bat
    )
) else (
    echo.
    echo Goodbye!
    timeout 2 >nul
)

exit /b 0