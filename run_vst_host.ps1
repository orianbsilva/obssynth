# Run VST GUI Host with local build
param(
    [switch]$Build,
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$PluginPath = "build\VST3\$Config\ObsSynth.vst3"
$HostExe = "build\bin\$Config\vst_gui_host.exe"

Write-Host ""
Write-Host "  VST GUI Host Runner" -ForegroundColor Cyan
Write-Host "  ===================" -ForegroundColor Cyan
Write-Host ""

if ($Build) {
    Write-Host "  [BUILD] Building ObsSynth + vst_gui_host..." -ForegroundColor Yellow
    cmake --build build --config $Config --target ObsSynth --parallel 8
    if ($LASTEXITCODE -ne 0) { Write-Host "  [ERROR] Build failed!" -ForegroundColor Red; exit 1 }
    cmake --build build --config $Config --target vst_gui_host --parallel 8
    if ($LASTEXITCODE -ne 0) { Write-Host "  [ERROR] Build failed!" -ForegroundColor Red; exit 1 }
    Write-Host "  [OK] Build complete" -ForegroundColor Green
    Write-Host ""
}

if (-not (Test-Path $PluginPath)) {
    Write-Host "  [ERROR] Plugin not found: $PluginPath" -ForegroundColor Red
    Write-Host "  Run: .\run_vst_host.ps1 -Build" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $HostExe)) {
    Write-Host "  [ERROR] Host not found: $HostExe" -ForegroundColor Red
    Write-Host "  Run: .\run_vst_host.ps1 -Build" -ForegroundColor Yellow
    exit 1
}

Write-Host "  Plugin: $PluginPath" -ForegroundColor Gray
Write-Host "  Host:   $HostExe" -ForegroundColor Gray
Write-Host ""
Write-Host "  Starting..." -ForegroundColor Green
Write-Host ""

& $HostExe $PluginPath
