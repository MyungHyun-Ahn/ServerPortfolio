param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$refactoringServerRoot = Split-Path -Parent $scriptDirectory

$generatePacketsScript = Join-Path $scriptDirectory "Generate-Packets.ps1"
$generateConfigsScript = Join-Path $scriptDirectory "Generate-Configs.ps1"

if (-not (Test-Path $generatePacketsScript))
{
    throw "Generate-Packets script not found: $generatePacketsScript"
}

if (-not (Test-Path $generateConfigsScript))
{
    throw "Generate-Configs script not found: $generateConfigsScript"
}

Write-Host "Running packet/codegen pipeline..." -ForegroundColor Cyan
& powershell -ExecutionPolicy Bypass -File $generatePacketsScript -Configuration $Configuration

Write-Host "Running config/codegen pipeline..." -ForegroundColor Cyan
& powershell -ExecutionPolicy Bypass -File $generateConfigsScript -Configuration $Configuration

Write-Host "Code generation completed." -ForegroundColor Green
