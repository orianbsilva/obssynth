@echo off
echo Synth Project - CMake Demo Runner
echo =================================
echo.

if not exist build\bin\Release\*.exe (
    echo Executables not found. Building project first...
    call build_cmake.bat
    if %ERRORLEVEL% neq 0 (
        echo Build failed!
        
        exit /b 1
    )
)

echo Available demos:
echo 1. test_a4.exe         - A4 pitch test (plays 440Hz in all wave types)
echo 2. test_midi.exe       - MIDI synthesizer (requires MIDI input device)
echo 3. simple_wav_example.exe - Generate WAV files of all wave types
echo 4. test_wave_print.exe - Display waveform data visualization
echo 5. Build project
echo 6. Clean project
echo 0. Exit
echo.

set /p choice="Select demo (0-6): "

if "%choice%"=="1" (
    echo.
    echo Running A4 Pitch Test...
    echo This will play A4 (440Hz) in sine, sawtooth, and square waves for 2 seconds each.
    echo.
    
    build\bin\Release\test_a4.exe
) else if "%choice%"=="2" (
    echo.
    echo Running MIDI Synthesizer...
    echo Make sure you have a MIDI input device connected.
    echo.
    
    build\bin\Release\test_midi.exe
) else if "%choice%"=="3" (
    echo.
    echo Running WAV File Generator...
    echo This will create sine_wave.wav, sawtooth_wave.wav, and square_wave.wav
    echo.
    
    build\bin\Release\simple_wav_example.exe
) else if "%choice%"=="4" (
    echo.
    echo Running Waveform Visualization...
    echo This will display ASCII visualization of waveform data.
    echo.
    
    build\bin\Release\test_wave_print.exe
) else if "%choice%"=="5" (
    echo.
    echo Building project...
    call build_cmake.bat
) else if "%choice%"=="6" (
    echo.
    echo Cleaning project...
    call clean_cmake.bat
) else if "%choice%"=="0" (
    echo Goodbye!
    exit /b 0
) else (
    echo Invalid choice!
    
    goto :eof
)

echo.
echo Demo completed. Press any key to continue...
 > nul
