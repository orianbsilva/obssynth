@echo off
setlocal enabledelayedexpansion

echo =====================================================
echo VST3 Plugin Installer for ObsSynth
echo =====================================================
echo.

:: Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting administrator privileges...
    echo.
    
    :: Re-run this script with administrator privileges
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b 0
)

:: Define paths
set "SOURCE_VST=%~dp0build\VST3\Release\ObsSynth.vst3"
set "DEST_VST=C:\Program Files\Common Files\VST3\ObsSynth.vst3"

echo Source: %SOURCE_VST%
echo Destination: %DEST_VST%
echo.

:: Check if source VST exists
if not exist "%SOURCE_VST%" (
    echo ERROR: VST plugin not found at: %SOURCE_VST%
    echo Please build the plugin first using: cmake --build build --config Release
    echo.
    pause
    exit /b 1
)

:: Create VST3 directory if it doesn't exist
if not exist "C:\Program Files\Common Files\VST3\" (
    echo Creating VST3 directory...
    mkdir "C:\Program Files\Common Files\VST3\"
)

:: Remove existing installation if present
if exist "%DEST_VST%" (
    echo Removing existing installation...
    rmdir /s /q "%DEST_VST%"
    if exist "%DEST_VST%" (
        echo ERROR: Failed to remove existing installation
        echo Please close any DAWs using the plugin and try again
        echo.
        pause
        exit /b 1
    )
)

:: Copy the VST plugin
echo Installing VST plugin...
xcopy "%SOURCE_VST%" "%DEST_VST%\" /e /i /h /y
if %errorLevel% neq 0 (
    echo ERROR: Failed to copy VST plugin
    echo.
    pause
    exit /b 1
)

:: Verify installation
if exist "%DEST_VST%\Contents\x86_64-win\ObsSynth.vst3" (
    echo.
    echo =====================================================
    echo SUCCESS: ObsSynth VST3 installed successfully!
    echo =====================================================
    echo.
    echo Location: %DEST_VST%
    echo.
    echo The plugin should now appear in your DAW's instrument list.
    echo You may need to restart your DAW to refresh the plugin database.
    echo.
    echo Category: Instrument ^| Synth
    echo Features: 32-voice polyphonic synthesizer with ADSR envelopes
    echo.
) else (
    echo ERROR: Installation verification failed
    echo The plugin files may not have been copied correctly
    echo.
    pause
    exit /b 1
)

echo Press any key to exit...
pause >nul