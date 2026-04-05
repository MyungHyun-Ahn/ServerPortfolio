@echo off
setlocal
set SCRIPT_DIR=%~dp0
powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%Run-RioDispatchComparisonSequence.ps1" %*
endlocal
