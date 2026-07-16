# Rebuild RebelstarDelta editor module
$ErrorActionPreference = "Stop"
$UAT = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$Project = "C:\UE\RebelstarDelta\RebelstarDelta.uproject"

if (-not (Test-Path $UAT)) { throw "Build.bat not found" }

Write-Host "Building RebelstarDeltaEditor (Win64 Development)..." -ForegroundColor Cyan
& $UAT RebelstarDeltaEditor Win64 Development -Project="$Project" -WaitMutex -MaxParallelActions=4
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit $LASTEXITCODE" }
Write-Host "Build OK" -ForegroundColor Green
