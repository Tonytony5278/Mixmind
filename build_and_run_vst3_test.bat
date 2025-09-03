@echo off
echo Building VST3 E2E Test...

REM Set up Visual Studio environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

REM Compile the test
cd tests\e2e
cl /std:c++20 /EHsc test_vst3_simple.cpp /Fe:vst3_e2e_test.exe

if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful! Running test...
cd ..\..

REM Run the test
tests\e2e\vst3_e2e_test.exe

echo Test completed. Check artifacts directory for results.
pause