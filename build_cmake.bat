@echo off
echo Building Synth project with CMake...

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake using Visual Studio 2026
echo Configuring project...
cmake .. -G "Visual Studio 18 2026" -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build . --config Release --parallel 8

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    
    exit /b 1
)

echo.
echo Build completed successfully!
echo Executables are in: build\bin\
echo.

REM List the built executables
if exist bin\*.exe (
    echo Built executables:
    dir bin\*.exe /b
) else (
    echo Built executables:
    dir Release\*.exe /b 2>nul
    if %ERRORLEVEL% neq 0 (
        dir Debug\*.exe /b 2>nul
    )
)

echo.
echo To run the MIDI test: build\bin\test_midi.exe (or build\Release\test_midi.exe)
echo.
