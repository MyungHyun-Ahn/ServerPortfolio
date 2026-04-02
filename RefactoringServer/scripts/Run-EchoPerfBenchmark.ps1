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

$statLines = Get-Content $serverConsoleLog | Where-Object { $_ -like "[EchoStats]*" }
if ($statLines.Count -gt 0)
{
    $patterns = @{
        AvgAcceptTPS = 'acceptTPS=(?<value>[0-9]+(?:\.[0-9]+)?)'
        AvgRecvTPS = 'recvTPS=(?<value>[0-9]+(?:\.[0-9]+)?)'
        AvgSendTPS = 'sendTPS=(?<value>[0-9]+(?:\.[0-9]+)?)'
        AvgRecvBps = 'recvBps=(?<value>[0-9]+(?:\.[0-9]+)?)'
        AvgSendBps = 'sendBps=(?<value>[0-9]+(?:\.[0-9]+)?)'
        AvgWsaSendTPS = 'wsaSendTPS=(?<value>[0-9]+(?:\.[0-9]+)?)'
        AvgWsaRecvTPS = 'wsaRecvTPS=(?<value>[0-9]+(?:\.[0-9]+)?)'
        AvgCpuPercent = 'cpuPercent=(?<value>[0-9]+(?:\.[0-9]+)?)'
        AvgWorkingSetMB = 'workingSetMB=(?<value>[0-9]+(?:\.[0-9]+)?)'
        PeakWorkingSetMB = 'peakWorkingSetMB=(?<value>[0-9]+(?:\.[0-9]+)?)'
    }

    $summary = [ordered]@{}
    foreach ($entry in $patterns.GetEnumerator())
    {
        $values = New-Object System.Collections.Generic.List[double]
        foreach ($line in $statLines)
        {
            $match = [regex]::Match($line, $entry.Value)
            if ($match.Success)
            {
                $values.Add([double]$match.Groups['value'].Value)
            }
        }

        if ($values.Count -eq 0)
        {
            continue
        }

        if ($entry.Key -eq 'PeakWorkingSetMB')
        {
            $summary[$entry.Key] = ($values | Measure-Object -Maximum).Maximum
        }
        else
        {
            $summary[$entry.Key] = ($values | Measure-Object -Average).Average
        }
    }

    Write-Host ""
    Write-Host "Benchmark summary:"
    foreach ($entry in $summary.GetEnumerator())
    {
        Write-Host ("{0}={1:N2}" -f $entry.Key, [double]$entry.Value)
    }
}
