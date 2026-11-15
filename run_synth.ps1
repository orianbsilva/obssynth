<#
.SYNOPSIS
    Executa o VST Host Test com ASIO + MIDI
.DESCRIPTION
    Script para iniciar o sintetizador de desenvolvimento sem DAW.
    Suporta entrada MIDI real e saída ASIO de baixa latência.
.PARAMETER Build
    Recompila o projeto antes de executar
.PARAMETER Debug
    Usa build Debug em vez de Release
.PARAMETER ListMidi
    Lista dispositivos MIDI disponíveis e sai
.EXAMPLE
    .\run_synth.ps1
    Executa o sintetizador (build Release)
.EXAMPLE
    .\run_synth.ps1 -Build
    Recompila e executa
.EXAMPLE
    .\run_synth.ps1 -Debug -Build
    Recompila em Debug e executa
#>
param(
    [switch]$Build,
    [switch]$Debug,
    [switch]$ListMidi,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$Config = if ($Debug) { "Debug" } else { "Release" }
$Exe = "build\bin\$Config\vst_host_test.exe"

# Banner
Write-Host ""
Write-Host "  ╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "  ║       VST HOST TEST - ASIO + MIDI Edition        ║" -ForegroundColor Cyan  
Write-Host "  ║         ObsSynth Development Environment         ║" -ForegroundColor Cyan
Write-Host "  ╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

if ($Help) {
    Write-Host "  Uso: .\run_synth.ps1 [-Build] [-Debug] [-Help]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  Parâmetros:" -ForegroundColor White
    Write-Host "    -Build     Recompila antes de executar"
    Write-Host "    -Debug     Usa configuração Debug"
    Write-Host "    -Help      Mostra esta ajuda"
    Write-Host ""
    Write-Host "  Controles do Sintetizador:" -ForegroundColor White
    Write-Host "    A-K, Q-I   Tocar notas (sem MIDI)"
    Write-Host "    1-5        Waveform (Sine/Saw/Square/Tri/FM)"
    Write-Host "    +/-        Volume up/down"
    Write-Host "    M          Métricas de performance"
    Write-Host "    P          Status atual"
    Write-Host "    X          Exportar 3 seg para WAV"
    Write-Host "    ESC        Sair"
    Write-Host ""
    Write-Host "  MIDI CC Mapping:" -ForegroundColor White
    Write-Host "    CC 7   → Volume"
    Write-Host "    CC 73  → Attack"
    Write-Host "    CC 75  → Decay"
    Write-Host "    CC 76  → Sustain"
    Write-Host "    CC 72  → Release"
    Write-Host ""
    exit 0
}

# Build se solicitado
if ($Build) {
    Write-Host "  [BUILD] Compilando vst_host_test ($Config)..." -ForegroundColor Yellow
    $buildResult = cmake --build build --config $Config --target vst_host_test --parallel 8 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERRO] Falha na compilação!" -ForegroundColor Red
        Write-Host $buildResult
        exit 1
    }
    Write-Host "  [OK] Build concluído" -ForegroundColor Green
    Write-Host ""
}

# Verifica se executável existe
if (-not (Test-Path $Exe)) {
    Write-Host "  [ERRO] Executável não encontrado: $Exe" -ForegroundColor Red
    Write-Host "         Execute: .\run_synth.ps1 -Build" -ForegroundColor Yellow
    exit 1
}

Write-Host "  Config: $Config" -ForegroundColor Gray
Write-Host "  Exe:    $Exe" -ForegroundColor Gray
Write-Host ""
Write-Host "  Pressione ESC dentro do programa para sair" -ForegroundColor DarkGray
Write-Host "  ─────────────────────────────────────────────────────" -ForegroundColor DarkGray
Write-Host ""

# Executa
& $Exe

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "  [WARN] Programa encerrou com código $LASTEXITCODE" -ForegroundColor Yellow
}
