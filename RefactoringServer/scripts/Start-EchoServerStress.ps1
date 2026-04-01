param(
    [int]$SendThreadCount = 8,
    [int]$ResponsesPerThread = 8,
    [switch]$PassThru
)

$ErrorActionPreference = "Stop"

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$refactoringServerRoot = Split-Path -Parent $scriptDirectory
$outDirectory = Join-Path $refactoringServerRoot "Out"
$echoServerPath = Join-Path $outDirectory "EchoServer.exe"

if (-not (Test-Path $echoServerPath))
{
    throw "EchoServer.exe not found: $echoServerPath"
}

New-Item -ItemType Directory -Force -Path $outDirectory | Out-Null

$arguments = @(
    "--headless"
    "--send-thread-count", $SendThreadCount
    "--responses-per-thread", $ResponsesPerThread
)

Write-Host "EchoServer starting in visible console mode..."
Write-Host "network logs: $(Join-Path $outDirectory 'logs\\NetworkLib')"
Write-Host "echo logs: $(Join-Path $outDirectory 'logs\\EchoServer')"
Write-Host "Close the server console or press Ctrl+C there to stop it."

& $echoServerPath @arguments
