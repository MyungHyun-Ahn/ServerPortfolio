param(
    [int]$DurationSeconds = 7200,
    [int]$Sessions = 100,
    [int]$Count = 1,
    [int]$IntervalMs = 200,
    [int]$PacketsPerSend = 2,
    [int]$RecvTimeoutMs = 5000,
    [int]$RacePeriod = 100,
    [ValidateSet("switch", "sleep0", "yield")]
    [string]$RaceMode = "sleep0"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root "Out"
$serverExe = Join-Path $outDir "EchoServer.exe"
$clientExe = Join-Path $outDir "EchoClient.exe"
$logDir = Join-Path $outDir "contents-race-validation"

New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$serverLog = Join-Path $logDir "server_$timestamp.log"
$serverErr = Join-Path $logDir "server_$timestamp.err.log"
$clientLog = Join-Path $logDir "client_$timestamp.log"
$clientErr = Join-Path $logDir "client_$timestamp.err.log"

Get-Process EchoServer, EchoClient -ErrorAction SilentlyContinue | Stop-Process -Force

$serverArgs = @(
    "--headless",
    "--bootstrap-trace",
    "--contents-race-injection",
    "--contents-race-mode", $RaceMode,
    "--contents-race-period", $RacePeriod,
    "--contents-fail-fast"
)

$server = Start-Process `
    -FilePath $serverExe `
    -ArgumentList $serverArgs `
    -RedirectStandardOutput $serverLog `
    -RedirectStandardError $serverErr `
    -PassThru

Start-Sleep -Seconds 2

if ($server.HasExited)
{
    throw "EchoServer exited before client start. See $serverLog"
}

$clientArgs = @(
    "--sessions", $Sessions,
    "--count", $Count,
    "--response-thread-count", 1,
    "--responses-per-thread", 1,
    "--hold-seconds", $DurationSeconds,
    "--interval-ms", $IntervalMs,
    "--packets-per-send", $PacketsPerSend,
    "--recv-timeout-ms", $RecvTimeoutMs,
    "--bootstrap-trace",
    "--quiet"
)

$client = Start-Process `
    -FilePath $clientExe `
    -ArgumentList $clientArgs `
    -RedirectStandardOutput $clientLog `
    -RedirectStandardError $clientErr `
    -PassThru

while (-not $client.HasExited)
{
    Start-Sleep -Seconds 1

    if ($server.HasExited)
    {
        $client | Stop-Process -Force -ErrorAction SilentlyContinue
        throw "EchoServer terminated early. See $serverLog and $serverErr"
    }
}

$client.WaitForExit()
$client.Refresh()

if (-not $server.HasExited)
{
    $server | Stop-Process -Force -ErrorAction SilentlyContinue
}

$clientSucceeded = Select-String -Path $clientLog -Pattern 'echo validation succeeded\.' -Quiet
$clientExitCode = $client.ExitCode
if (-not $clientSucceeded -and $null -ne $clientExitCode -and $clientExitCode -ne 0)
{
    throw "EchoClient failed with exit code $clientExitCode. See $clientLog"
}

if (-not $clientSucceeded -and $null -eq $clientExitCode)
{
    throw "EchoClient did not produce a success marker and exit code was unavailable. See $clientLog"
}

Write-Host "ContentsRuntime race validation finished successfully."
Write-Host "Server log: $serverLog"
Write-Host "Client log: $clientLog"
Write-Host "Client err: $clientErr"
