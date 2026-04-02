param(
    [int]$SessionCount = 100,
    [int]$RequestCount = 32,
    [int]$PayloadSize = 128,
    [int]$HoldSeconds = 300,
    [int]$IntervalMs = 100,
    [int]$PacketsPerSend = 4,
    [int]$ReconnectProbabilityPercent = 5,
    [int]$ReconnectDelayMs = 100,
    [int]$SendChunkSize = 7,
    [int]$SendChunkDelayMs = 1,
    [int]$RecvBufferSize = 13,
    [int]$ResponseThreadCount = 1,
    [int]$ResponsesPerThread = 1,
    [switch]$DisablePagePool,
    [int]$PageSize = 4096
)

$ErrorActionPreference = "Stop"

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $scriptDirectory
$outDirectory = Join-Path $root "Out"
$perfDirectory = Join-Path $outDirectory "perf"
New-Item -ItemType Directory -Force -Path $perfDirectory | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$serverConsoleLog = Join-Path $perfDirectory "echo_server_perf_$timestamp.log"
$clientStdout = Join-Path $perfDirectory "echo_client_perf_$timestamp.log"
$clientStderr = Join-Path $perfDirectory "echo_client_perf_$timestamp.err.log"

$echoServer = Join-Path $outDirectory "EchoServer.exe"
$echoClient = Join-Path $outDirectory "EchoClient.exe"

if (-not (Test-Path $echoServer)) { throw "EchoServer.exe not found: $echoServer" }
if (-not (Test-Path $echoClient)) { throw "EchoClient.exe not found: $echoClient" }

$serverArguments = @("--headless", "--send-thread-count", "1", "--responses-per-thread", "1", "--page-size", $PageSize.ToString())
if ($DisablePagePool)
{
    $serverArguments += "--disable-page-pool"
}

$server = Start-Process `
    -FilePath $echoServer `
    -ArgumentList $serverArguments `
    -RedirectStandardOutput $serverConsoleLog `
    -PassThru

Start-Sleep -Seconds 2

try
{
    $clientArguments = @(
        "--sessions", $SessionCount,
        "--count", $RequestCount,
        "--payload-size", $PayloadSize,
        "--send-chunk-size", $SendChunkSize,
        "--send-chunk-delay-ms", $SendChunkDelayMs,
        "--recv-buffer-size", $RecvBufferSize,
        "--response-thread-count", $ResponseThreadCount,
        "--responses-per-thread", $ResponsesPerThread,
        "--hold-seconds", $HoldSeconds,
        "--interval-ms", $IntervalMs,
        "--packets-per-send", $PacketsPerSend,
        "--reconnect-probability-percent", $ReconnectProbabilityPercent,
        "--reconnect-delay-ms", $ReconnectDelayMs,
        "--page-size", $PageSize,
        "--quiet"
    )
    if ($DisablePagePool)
    {
        $clientArguments += "--disable-page-pool"
    }

    & $echoClient `
        @clientArguments *>> $clientStdout 2>> $clientStderr

    if ($LASTEXITCODE -ne 0)
    {
        throw "EchoClient failed with exit code $LASTEXITCODE"
    }
}
finally
{
    if ($null -ne $server -and -not $server.HasExited)
    {
        Stop-Process -Id $server.Id -Force
    }
}

Write-Host "Benchmark completed."
Write-Host "server log: $serverConsoleLog"
Write-Host "client stdout: $clientStdout"
Write-Host "client stderr: $clientStderr"
