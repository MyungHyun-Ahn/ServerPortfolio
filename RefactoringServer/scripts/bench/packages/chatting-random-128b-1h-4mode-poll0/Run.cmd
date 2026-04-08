@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0Run.ps1" %*
endlocal
