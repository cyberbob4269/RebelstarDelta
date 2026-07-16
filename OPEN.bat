@echo off
cd /d "%~dp0"
echo Opening Rebelstar Delta...
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\open_editor.ps1"
if errorlevel 1 (
  echo OPEN FAILED
  pause
  exit /b 1
)
