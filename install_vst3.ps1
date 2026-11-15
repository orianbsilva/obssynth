<#
.SYNOPSIS
    Instala o VST3 ObsSynth no sistema Windows
.DESCRIPTION
    Copia o plugin VST3 para a pasta padrão do sistema onde DAWs podem encontrá-lo.
    Requer privilégios de administrador para instalar em Program Files.
.PARAMETER User
    Instala na pasta do usuário (não requer admin)
.PARAMETER System
    Instala em Program Files (requer admin)
.PARAMETER Build
    Compila o projeto antes de instalar
.PARAMETER Uninstall
    Remove o plugin instalado
.EXAMPLE
    .\install_vst3.ps1 -User
    Instala na pasta do usuário
.EXAMPLE
    .\install_vst3.ps1 -System
    Instala em Program Files (requer admin)
#>
param(
    [switch]$User,
    [switch]$System,
    [switch]$Build,
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$PluginName = "ObsSynth.vst3"
$SourcePath = "build\VST3\Release\$PluginName"
$UserVST3 = "$env:LOCALAPPDATA\Programs\Common\VST3"
$SystemVST3 = "C:\Program Files\Common Files\VST3"

# Banner
Write-Host ""
Write-Host "  ======================================================" -ForegroundColor Cyan
Write-Host "            ObsSynth VST3 Installer                    " -ForegroundColor Cyan
Write-Host "  ======================================================" -ForegroundColor Cyan
Write-Host ""

# Default to User install
if (-not $User -and -not $System) {
    $User = $true
}

$TargetPath = if ($System) { $SystemVST3 } else { $UserVST3 }
$FullTarget = Join-Path $TargetPath $PluginName

# Build if requested
if ($Build) {
    Write-Host "  [BUILD] Building ObsSynth..." -ForegroundColor Yellow
    cmake --build build --config Release --target ObsSynth --parallel 8
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERROR] Build failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "  [OK] Build complete" -ForegroundColor Green
    Write-Host ""
}

# Check source exists
if (-not (Test-Path $SourcePath)) {
    Write-Host "  [ERROR] Plugin not found: $SourcePath" -ForegroundColor Red
    Write-Host "         Run: .\install_vst3.ps1 -Build" -ForegroundColor Yellow
    exit 1
}

# Uninstall mode
if ($Uninstall) {
    Write-Host "  Uninstalling from: $FullTarget" -ForegroundColor Yellow
    
    if (Test-Path $FullTarget) {
        try {
            Remove-Item -Path $FullTarget -Recurse -Force
            Write-Host "  [OK] Plugin removed" -ForegroundColor Green
        }
        catch {
            Write-Host "  [ERROR] Failed to remove: $_" -ForegroundColor Red
            exit 1
        }
    }
    else {
        Write-Host "  [INFO] Plugin was not installed" -ForegroundColor Gray
    }
    exit 0
}

# Install mode
Write-Host "  Source:  $SourcePath"
Write-Host "  Target:  $FullTarget"
Write-Host ""

# Check admin for System install
if ($System) {
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Write-Host "  [ERROR] System install requires administrator privileges!" -ForegroundColor Red
        Write-Host "         Run PowerShell as Administrator or use -User" -ForegroundColor Yellow
        exit 1
    }
}

# Create target directory if needed
if (-not (Test-Path $TargetPath)) {
    Write-Host "  Creating folder: $TargetPath" -ForegroundColor Gray
    New-Item -ItemType Directory -Path $TargetPath -Force | Out-Null
}

# Remove old installation if exists
if (Test-Path $FullTarget) {
    Write-Host "  Removing previous installation..." -ForegroundColor Gray
    Remove-Item -Path $FullTarget -Recurse -Force
}

# Copy plugin
Write-Host "  Installing plugin..." -ForegroundColor Yellow
try {
    Copy-Item -Path $SourcePath -Destination $FullTarget -Recurse -Force
    Write-Host "  [OK] Plugin installed successfully!" -ForegroundColor Green
}
catch {
    Write-Host "  [ERROR] Failed to copy: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "  ------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""
Write-Host "  ObsSynth installed at:" -ForegroundColor White
Write-Host "  $FullTarget" -ForegroundColor Cyan
Write-Host ""
Write-Host "  To use:" -ForegroundColor White
Write-Host "  1. Open your DAW (Reaper, FL Studio, Ableton, etc.)" -ForegroundColor Gray
Write-Host "  2. Search for ObsSynth in VST3 instruments" -ForegroundColor Gray
Write-Host "  3. Add to a MIDI track" -ForegroundColor Gray
Write-Host ""

# List installed files
$files = Get-ChildItem -Path $FullTarget -Recurse -File
Write-Host ('  Files installed: ' + $files.Count) -ForegroundColor DarkGray
