@echo off
REM ============================================================
REM  VST Host Test - Sintetizador com ASIO + MIDI
REM ============================================================
REM  Uso: run_synth.bat [opções]
REM
REM  Opções:
REM    /build   - Recompila antes de executar
REM    /debug   - Usa build Debug em vez de Release
REM    /help    - Mostra esta ajuda
REM ============================================================

setlocal enabledelayedexpansion

set BUILD_FIRST=0
set CONFIG=Release

:parse_args
if "%~1"=="" goto run
if /i "%~1"=="/build" set BUILD_FIRST=1& shift& goto parse_args
if /i "%~1"=="/debug" set CONFIG=Debug& shift& goto parse_args
if /i "%~1"=="/help" goto show_help
if /i "%~1"=="-h" goto show_help
if /i "%~1"=="--help" goto show_help
shift
goto parse_args

:show_help
echo.
echo  VST Host Test - Sintetizador ASIO + MIDI
echo  =========================================
echo.
echo  Uso: run_synth.bat [opcoes]
echo.
echo  Opcoes:
echo    /build   Recompila antes de executar
echo    /debug   Usa build Debug
echo    /help    Mostra esta ajuda
echo.
echo  Controles do Sintetizador:
echo    A-K, Q-I   Tocar notas (sem MIDI)
echo    1-5        Waveform (Sine/Saw/Square/Tri/FM)
echo    +/-        Volume
echo    M          Metricas de performance
echo    X          Exportar WAV
echo    ESC        Sair
echo.
echo  MIDI CC:
echo    CC 7   = Volume
echo    CC 73  = Attack
echo    CC 75  = Decay  
echo    CC 76  = Sustain
echo    CC 72  = Release
echo.
goto :eof

:run
cd /d "%~dp0"

if %BUILD_FIRST%==1 (
    echo [BUILD] Compilando vst_host_test...
    cmake --build build --config %CONFIG% --target vst_host_test --parallel 8
    if errorlevel 1 (
        echo [ERRO] Falha na compilacao!
        pause
        exit /b 1
    )
    echo.
)

set EXE=build\bin\%CONFIG%\vst_host_test.exe

if not exist "%EXE%" (
    echo [ERRO] Executavel nao encontrado: %EXE%
    echo        Execute: run_synth.bat /build
    pause
    exit /b 1
)

echo.
echo  Iniciando VST Host Test (%CONFIG%)...
echo  Pressione ESC para sair
echo.

"%EXE%"
