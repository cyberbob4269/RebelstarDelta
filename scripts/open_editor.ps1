# Open Rebelstar Delta editor (after module build)
$ErrorActionPreference = "Stop"
$Project = "C:\UE\RebelstarDelta\RebelstarDelta.uproject"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if (-not (Test-Path $Editor)) { throw "UE 5.8 editor not found: $Editor" }
if (-not (Test-Path $Project)) { throw "Project not found: $Project" }

$Dll = "C:\UE\RebelstarDelta\Binaries\Win64\UnrealEditor-RebelstarDelta.dll"
if (-not (Test-Path $Dll)) {
    Write-Host "Game module not built yet — running rebuild..." -ForegroundColor Yellow
    & "$PSScriptRoot\rebuild_game_module.ps1"
}

# Prefer Live Coding so agent patches load. Use rebuild_game_module.ps1 first if cold-start.
Write-Host "Launching UnrealEditor (Live Coding ON)..." -ForegroundColor Cyan
Start-Process -FilePath $Editor -ArgumentList "`"$Project`""
