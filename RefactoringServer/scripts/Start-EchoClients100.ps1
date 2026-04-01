param(
    [int]$SessionCount = 100,
    [int]$RequestCount = 32,
    [int]$PayloadSize = 128,
    [int]$SendChunkSize = 7,
    [int]$SendChunkDelayMs = 1,
    [int]$RecvBufferSize = 13,
    [int]$ResponseThreadCount = 8,
    [int]$ResponsesPerThread = 8,
    [int]$HoldSeconds = 7200,
    [int]$IntervalMs = 500,
    [int]$PacketsPerSend = 4,
    [int]$ReconnectProbabilityPercent = 5,
    [int]$ReconnectDelayMs = 100,
    [switch]$Quiet,
    [switch]$ShowWindow
)

$ErrorActionPreference = "Stop"

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$refactoringServerRoot = Split-Path -Parent $scriptDirectory
$outDirectory = Join-Path $refactoringServerRoot "Out"
$echoClientPath = Join-Path $outDirectory "EchoClient.exe"
$clientLogDirectory = Join-Path $outDirectory "stress-clients"
$stdoutPath = Join-Path $clientLogDirectory "echo_client_sessions.log"
$stderrPath = Join-Path $clientLogDirectory "echo_client_sessions.err.log"

if (-not (Test-Path $echoClientPath))
{
    throw "EchoClient.exe not found: $echoClientPath"
}

New-Item -ItemType Directory -Force -Path $clientLogDirectory | Out-Null

$arguments = @(
    "--sessions", $SessionCount
    "--count", $RequestCount
    "--payload-size", $PayloadSize
    "--send-chunk-size", $SendChunkSize
    "--send-chunk-delay-ms", $SendChunkDelayMs
    "--recv-buffer-size", $RecvBufferSize
    "--response-thread-count", $ResponseThreadCount
    "--responses-per-thread", $ResponsesPerThread
    "--hold-seconds", $HoldSeconds
    "--interval-ms", $IntervalMs
    "--packets-per-send", $PacketsPerSend
    "--reconnect-probability-percent", $ReconnectProbabilityPercent
    "--reconnect-delay-ms", $ReconnectDelayMs
)

if ($Quiet)
{
    $arguments += "--quiet"
}

$clientProcess = Start-Process `
    -FilePath $echoClientPath `
    -ArgumentList $arguments `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -WindowStyle $(if ($ShowWindow) { "Normal" } else { "Hidden" }) `
    -PassThru

Write-Host "EchoClient started."
Write-Host "pid: $($clientProcess.Id)"
Write-Host "session count: $SessionCount"
Write-Host "hold seconds: $HoldSeconds"
Write-Host "interval ms: $IntervalMs"
Write-Host "packets per send: $PacketsPerSend"
Write-Host "reconnect probability percent: $ReconnectProbabilityPercent"
Write-Host "stdout: $stdoutPath"
Write-Host "stderr: $stderrPath"
Write-Host "window mode: $(if ($ShowWindow) { 'normal' } else { 'hidden' })"
Write-Host "Use this to wait for completion:"
Write-Host '$client = Get-Process EchoClient -ErrorAction SilentlyContinue'
Write-Host '$client | Wait-Process'
