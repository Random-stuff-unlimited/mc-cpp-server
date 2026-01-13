# =============================================================================
# mc-server Run Script (PowerShell)
# Sets up test environment and runs the Minecraft server
# =============================================================================

param(
    [ValidateSet("debug", "release", "relwithdebinfo")]
    [string]$BuildType = "release"
)

$ErrorActionPreference = "Stop"

# Configuration
$TargetName = "mc-server"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$TestServerDir = Join-Path $ProjectRoot "test-server"
$BuildDir = Join-Path $ProjectRoot "build" $BuildType
$Executable = Join-Path $BuildDir "$TargetName.exe"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  mc-server Run Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if executable exists
if (-not (Test-Path $Executable)) {
    Write-Host "Executable not found. Building in $BuildType mode..." -ForegroundColor Yellow
    Push-Location $ProjectRoot
    try {
        cmake --workflow --preset $BuildType
    }
    finally {
        Pop-Location
    }
}

# Verify executable exists after build
if (-not (Test-Path $Executable)) {
    Write-Host "Error: Failed to build executable" -ForegroundColor Red
    exit 1
}

Write-Host "Setting up test-server environment..." -ForegroundColor Blue

# Create test-server directory
if (-not (Test-Path $TestServerDir)) {
    New-Item -ItemType Directory -Path $TestServerDir -Force | Out-Null
}

# Copy executable
Write-Host "  Copying executable..." -ForegroundColor Cyan
Copy-Item $Executable -Destination (Join-Path $TestServerDir "$TargetName.exe") -Force

# Copy config.json if it exists
$ConfigPath = Join-Path $ProjectRoot "config.json"
if (Test-Path $ConfigPath) {
    Write-Host "  Copying config.json..." -ForegroundColor Cyan
    Copy-Item $ConfigPath -Destination $TestServerDir -Force
}
else {
    Write-Host "  Warning: config.json not found, skipping..." -ForegroundColor Yellow
}

# Copy world folder if it exists
$WorldPath = Join-Path $ProjectRoot "world"
if (Test-Path $WorldPath) {
    Write-Host "  Copying world folder..." -ForegroundColor Cyan
    Copy-Item $WorldPath -Destination $TestServerDir -Recurse -Force
}
else {
    Write-Host "  Warning: world folder not found, skipping..." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Test environment ready!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Running $TargetName ($BuildType)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Run the server
Push-Location $TestServerDir
try {
    & ".\$TargetName.exe"
}
finally {
    Pop-Location
}
