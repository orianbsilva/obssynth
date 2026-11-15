@echo off
REM SYNTH_CLI Quick Launch Script
REM Professional MIDI Synthesizer

setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "SYNTH_CLI=%BUILD_DIR%\synth_cli.exe"

echo SYNTH_CLI - Professional MIDI Synthesizer
echo ========================================

REM Check if already built
if not exist "%SYNTH_CLI%" (
    echo Building synth_cli...
    if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
    cd /d "%BUILD_DIR%"
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
    echo Build complete!
    echo.
)

REM Launch synth_cli
echo Launching synth_cli...
echo Press Ctrl+C to exit the synthesizer
echo.
"%SYNTH_CLI%"
