<#
.SYNOPSIS
    Script PowerShell para build do projeto Synth com CMake.

.DESCRIPTION
    Compila o projeto Synth usando CMake e Visual Studio 2026.
    Substitui o build_cmake.bat para melhor integração com PowerShell.

.PARAMETER Config
    Configuração de build: Release (padrão) ou Debug

.PARAMETER Clean
    Limpa o diretório de build antes de compilar

.PARAMETER Parallel
    Número de jobs paralelos (padrão: 8)

.PARAMETER Verbose
    Mostra saída detalhada do build

.EXAMPLE
    .\build_cmake.ps1
    
.EXAMPLE
    .\build_cmake.ps1 -Config Debug -Clean -Verbose
#>

param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    
    [switch]$Clean,
    [int]$Parallel = 8,
    [switch]$Verbose
)

# Cores
function Write-Success { param($msg) Write-Host "✓ $msg" -ForegroundColor Green }
function Write-Info { param($msg) Write-Host "→ $msg" -ForegroundColor Cyan }
function Write-Step { param($msg) Write-Host "▶ $msg" -ForegroundColor Yellow }
function Write-ErrorMsg { param($msg) Write-Host "✗ $msg" -ForegroundColor Red }

$ErrorActionPreference = "Stop"
$BuildDir = "build"

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "              SYNTH BUILD SYSTEM (PowerShell)                   " -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$startTime = Get-Date

# Limpar se solicitado
if ($Clean) {
    Write-Step "Limpando diretório de build..."
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
    Write-Success "Diretório limpo"
}

# Criar diretório de build
if (-not (Test-Path $BuildDir)) {
    Write-Step "Criando diretório de build..."
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Push-Location $BuildDir

try {
    # Configurar com CMake
    Write-Step "Configurando projeto com CMake..."
    $cmakeArgs = @(
        ".."
        "-G", "Visual Studio 18 2026"
        "-DCMAKE_BUILD_TYPE=$Config"
    )
    
    if ($Verbose) {
        cmake @cmakeArgs
    }
    else {
        $output = cmake @cmakeArgs 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host $output
            throw "CMake configuration failed"
        }
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorMsg "Configuração CMake falhou!"
        Pop-Location
        exit 1
    }
    Write-Success "Configuração concluída"
    
    # Build
    Write-Step "Compilando projeto ($Config, $Parallel threads)..."
    $buildArgs = @(
        "--build", "."
        "--config", $Config
        "--parallel", $Parallel
    )
    
    cmake @buildArgs
    
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorMsg "Build falhou!"
        Pop-Location
        exit 1
    }
    
    Write-Success "Build concluído"
    
}
catch {
    Write-ErrorMsg "Erro: $_"
    Pop-Location
    exit 1
}

Pop-Location

$endTime = Get-Date
$duration = ($endTime - $startTime).TotalSeconds

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "                    BUILD CONCLUÍDO                             " -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host ""
Write-Host "⏱️  Tempo: $([math]::Round($duration, 1)) segundos" -ForegroundColor Gray
Write-Host "📁 Executáveis: build\bin\$Config\" -ForegroundColor Gray
Write-Host ""

# Listar executáveis
$binDir = "build\bin\$Config"
if (Test-Path $binDir) {
    $exeCount = (Get-ChildItem "$binDir\*.exe").Count
    Write-Info "$exeCount executáveis gerados"
    
    if ($Verbose) {
        Write-Host ""
        Get-ChildItem "$binDir\*.exe" | ForEach-Object {
            Write-Host "   - $($_.Name)" -ForegroundColor DarkGray
        }
    }
}

Write-Host ""
Write-Info "Para testar: .\build\bin\$Config\test_a4.exe"
Write-Info "Para MIDI:   .\build\bin\$Config\synth_cli.exe"
Write-Host ""

exit 0
