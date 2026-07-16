# Copy live training logs into Content/Data/Training + a timestamped archive folder
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
$Src = Join-Path $Root "Saved\AgentVision"
$Dst = Join-Path $Root "Content\Data\Training"
$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$Archive = Join-Path $Dst "archive_$Stamp"

New-Item -ItemType Directory -Force -Path $Dst | Out-Null
New-Item -ItemType Directory -Force -Path $Archive | Out-Null

$files = @(
  "training_series.csv",
  "latest_round.json"
)
foreach ($f in $files) {
  $p = Join-Path $Src $f
  if (Test-Path $p) {
    Copy-Item $p $Dst -Force
    Copy-Item $p $Archive -Force
    Write-Host "Copied $f"
  } else {
    Write-Host "Missing (ok if no PIE yet): $p" -ForegroundColor Yellow
  }
}

# Copy any battle reports present
Get-ChildItem -Path $Src -Filter "battle_report_*.txt" -ErrorAction SilentlyContinue |
  ForEach-Object { Copy-Item $_.FullName $Archive -Force }

# Summary line count
$csv = Join-Path $Dst "training_series.csv"
if (Test-Path $csv) {
  $n = (Get-Content $csv | Measure-Object -Line).Lines
  Write-Host "training_series.csv lines: $n (incl header)" -ForegroundColor Green
  Write-Host "Archive: $Archive"
}

Write-Host "Done. Durable data under Content\Data\Training"
