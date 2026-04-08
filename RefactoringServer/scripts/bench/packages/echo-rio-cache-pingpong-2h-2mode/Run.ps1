param(
    [string]$OutputLabel = "echo_rio_cache_pingpong_2h_2mode",
    [int]$HoldSeconds = 7200,
    [int]$SessionCount = 250,
    [int]$ConnectsPerSecond = 10,
    [int]$PayloadSize = 16,
    [int]$WorkerThreadCount = 4,
    [int]$RoomCount = 80,
    [int]$RoomCapacity = 4,
    [int]$Port = 19000
)

$ErrorActionPreference = "Stop"

function New-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)

    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Convert-ToYamlPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    return ($Path -replace "\\", "/")
}

function Write-Utf8Text {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $utf8NoBom)
}

function Render-Template {
    param(
        [Parameter(Mandatory = $true)][string]$TemplatePath,
        [Parameter(Mandatory = $true)][hashtable]$Tokens
    )

    $rendered = [System.IO.File]::ReadAllText($TemplatePath)
    foreach ($entry in $Tokens.GetEnumerator()) {
        $rendered = $rendered.Replace($entry.Key, [string]$entry.Value)
    }
    return $rendered
}

function Add-SequenceLine {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Line
    )

    Add-Content -Path $Path -Value $Line -Encoding utf8
}

function Try-ParseDouble {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }

    $parsed = 0.0
    if ([double]::TryParse(
        $Value,
        [System.Globalization.NumberStyles]::Float,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$parsed)) {
        return $parsed
    }

    return $null
}

function Parse-KeyValueLine {
    param([Parameter(Mandatory = $true)][string]$Line)

    $map = @{}
    foreach ($token in ($Line -split "\s+")) {
        if ($token.StartsWith("[")) {
            continue
        }

        $delimiterIndex = $token.IndexOf("=")
        if ($delimiterIndex -lt 0) {
            continue
        }

        $key = $token.Substring(0, $delimiterIndex)
        $value = $token.Substring($delimiterIndex + 1)
        $map[$key] = $value
    }

    return $map
}

function Wait-ForLogPattern {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [int]$TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $LogPath) {
            $match = Select-String -Path $LogPath -Pattern $Pattern -SimpleMatch -Quiet -ErrorAction SilentlyContinue
            if ($match) {
                return $true
            }
        }

        Start-Sleep -Milliseconds 250
    }

    return $false
}

function Wait-ProcessWithTimeout {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $timedOut = -not $Process.WaitForExit($TimeoutSeconds * 1000)
    $Process.Refresh()
    return [pscustomobject]@{
        TimedOut = $timedOut
        ExitCode = if ($Process.HasExited) { $Process.ExitCode } else { $null }
    }
}

function Stop-ProcessIfRunning {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) {
        return $null
    }

    if (-not $Process.HasExited) {
        try {
            Stop-Process -Id $Process.Id -Force -ErrorAction Stop
        }
        catch {
        }

        $Process.WaitForExit(10000) | Out-Null
    }

    if ($Process.HasExited) {
        return $Process.ExitCode
    }

    return $null
}

function Get-EchoStatsSamples {
    param([Parameter(Mandatory = $true)][string]$ServerStdoutLogPath)

    $samples = New-Object System.Collections.Generic.List[hashtable]
    if (-not (Test-Path $ServerStdoutLogPath)) {
        return $samples
    }

    foreach ($line in [System.IO.File]::ReadLines($ServerStdoutLogPath)) {
        if (-not $line.StartsWith("[EchoStats]")) {
            continue
        }

        $sample = Parse-KeyValueLine -Line $line
        $sessions = 0
        if ($sample.ContainsKey("sessions")) {
            [int]::TryParse($sample["sessions"], [ref]$sessions) | Out-Null
        }

        if ($sessions -le 0) {
            continue
        }

        $samples.Add($sample)
    }

    return $samples
}

function Measure-SampleAverage {
    param(
        [Parameter(Mandatory = $true)]$Samples,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $sum = 0.0
    $count = 0
    foreach ($sample in $Samples) {
        if (-not $sample.ContainsKey($Key)) {
            continue
        }

        $value = Try-ParseDouble -Value $sample[$Key]
        if ($null -eq $value) {
            continue
        }

        $sum += $value
        ++$count
    }

    if ($count -eq 0) {
        return $null
    }

    return $sum / $count
}

function Get-LastSampleValue {
    param(
        [Parameter(Mandatory = $true)]$Samples,
        [Parameter(Mandatory = $true)][string]$Key
    )

    if ($Samples.Count -eq 0) {
        return $null
    }

    $lastSample = $Samples[$Samples.Count - 1]
    if (-not $lastSample.ContainsKey($Key)) {
        return $null
    }

    return Try-ParseDouble -Value $lastSample[$Key]
}

function Get-ResponsesTotal {
    param([Parameter(Mandatory = $true)][string]$ClientStdoutLogPath)

    if (-not (Test-Path $ClientStdoutLogPath)) {
        return $null
    }

    foreach ($line in [System.IO.File]::ReadLines($ClientStdoutLogPath)) {
        if ($line -match "echo validation succeeded\..* responses=(\d+)") {
            return [int64]$Matches[1]
        }
    }

    return $null
}

function Get-RttStageSummary {
    param(
        [Parameter(Mandatory = $true)][string]$RttCsvPath,
        [Parameter(Mandatory = $true)][string]$Stage
    )

    if (-not (Test-Path $RttCsvPath)) {
        return $null
    }

    $rows = Import-Csv -Path $RttCsvPath
    $stageRows = @($rows | Where-Object { $_.stage -eq $Stage })
    if ($stageRows.Count -eq 0) {
        return $null
    }

    return $stageRows[$stageRows.Count - 1]
}

function Convert-RunSummaryToCsvRow {
    param([Parameter(Mandatory = $true)]$RunSummary)

    return [pscustomobject]@{
        Mode = $RunSummary.Mode
        Backend = $RunSummary.Backend
        RioSendDispatchMode = $RunSummary.RioSendDispatchMode
        HoldSeconds = $RunSummary.HoldSeconds
        SessionCount = $RunSummary.SessionCount
        PayloadSize = $RunSummary.PayloadSize
        ResponsesTotal = $RunSummary.ResponsesTotal
        ResponseAvgPerSec = $RunSummary.ResponseAvgPerSec
        AvgRecvTPS = $RunSummary.AvgRecvTPS
        AvgSendTPS = $RunSummary.AvgSendTPS
        AvgRecvBps = $RunSummary.AvgRecvBps
        AvgSendBps = $RunSummary.AvgSendBps
        AvgCpuPercent = $RunSummary.AvgCpuPercent
        EchoAvgMs = $RunSummary.EchoAvgMs
        EchoMaxMs = $RunSummary.EchoMaxMs
        FinalRioSendPrepareCount = $RunSummary.FinalRioSendPrepareCount
        FinalRioSendPrepareAvgNs = $RunSummary.FinalRioSendPrepareAvgNs
        FinalRioSendPrepareMaxNs = $RunSummary.FinalRioSendPrepareMaxNs
        FinalRioSendRingTouchCount = $RunSummary.FinalRioSendRingTouchCount
        FinalRioSendRingCrossThreadTouchCount = $RunSummary.FinalRioSendRingCrossThreadTouchCount
        FinalRioSendRingCrossThreadRatePercent = $RunSummary.FinalRioSendRingCrossThreadRatePercent
        FinalRioDirectSendRingLockCount = $RunSummary.FinalRioDirectSendRingLockCount
        FinalRioDirectSendRingLockWaitAvgNs = $RunSummary.FinalRioDirectSendRingLockWaitAvgNs
        FinalRioDirectSendRingLockWaitMaxNs = $RunSummary.FinalRioDirectSendRingLockWaitMaxNs
        FinalRioDirectSendRingLockHoldAvgNs = $RunSummary.FinalRioDirectSendRingLockHoldAvgNs
        FinalRioDirectSendRingLockHoldMaxNs = $RunSummary.FinalRioDirectSendRingLockHoldMaxNs
        SampleCount = $RunSummary.SampleCount
        ServerReady = $RunSummary.ServerReady
        ClientTimedOut = $RunSummary.ClientTimedOut
        ServerExitCode = $RunSummary.ServerExitCode
        ClientExitCode = $RunSummary.ClientExitCode
        Succeeded = $RunSummary.Succeeded
        OutputDirectory = $RunSummary.OutputDirectory
    }
}

$packageRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$serverExePath = Join-Path $packageRoot "Out\\EchoServer\\EchoServer.exe"
$clientExePath = Join-Path $packageRoot "Out\\EchoClient\\EchoClient.exe"
$serverTemplatePath = Join-Path $packageRoot "Config\\Server\\EchoServer.template.yaml"
$clientTemplatePath = Join-Path $packageRoot "Config\\Client\\EchoClient.template.yaml"

if (-not (Test-Path $serverExePath)) {
    throw "EchoServer executable not found: $serverExePath"
}

if (-not (Test-Path $clientExePath)) {
    throw "EchoClient executable not found: $clientExePath"
}

$sequenceTimestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$benchRoot = Join-Path $packageRoot "Out\\bench"
$sequenceDirectory = Join-Path $benchRoot ("{0}_{1}" -f $sequenceTimestamp, $OutputLabel)
New-Directory -Path $sequenceDirectory

$sequenceLogPath = Join-Path $sequenceDirectory "sequence.log"
Add-SequenceLine -Path $sequenceLogPath -Line ("start={0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
Add-SequenceLine -Path $sequenceLogPath -Line ("session_count={0}" -f $SessionCount)
Add-SequenceLine -Path $sequenceLogPath -Line ("hold_seconds={0}" -f $HoldSeconds)
Add-SequenceLine -Path $sequenceLogPath -Line ("payload_size={0}" -f $PayloadSize)
Add-SequenceLine -Path $sequenceLogPath -Line ("connects_per_second={0}" -f $ConnectsPerSecond)
Add-SequenceLine -Path $sequenceLogPath -Line ("room_count={0}" -f $RoomCount)
Add-SequenceLine -Path $sequenceLogPath -Line ("room_capacity={0}" -f $RoomCapacity)
Add-SequenceLine -Path $sequenceLogPath -Line ("worker_thread_count={0}" -f $WorkerThreadCount)
Add-SequenceLine -Path $sequenceLogPath -Line "mode_order=RioDirect,RioOwnerThread"

$runModes = @(
    @{
        Mode = "RioDirect"
        DispatchMode = "Direct"
        RunDirectoryName = "rio_direct_2h"
    },
    @{
        Mode = "RioOwnerThread"
        DispatchMode = "OwnerThread"
        RunDirectoryName = "rio_owner_thread_2h"
    }
)

$summaryRows = New-Object System.Collections.Generic.List[object]
$failedRuns = New-Object System.Collections.Generic.List[string]

foreach ($runMode in $runModes) {
    $runDirectory = Join-Path $sequenceDirectory $runMode.RunDirectoryName
    $serverLogsDirectory = Join-Path $runDirectory "server_logs"
    New-Directory -Path $runDirectory
    New-Directory -Path $serverLogsDirectory

    $serverConfigPath = Join-Path $runDirectory ("EchoServer.{0}.yaml" -f $runMode.Mode)
    $clientConfigPath = Join-Path $runDirectory ("EchoClient.{0}.yaml" -f $runMode.Mode)
    $serverStdoutLogPath = Join-Path $runDirectory "server.stdout.log"
    $serverStderrLogPath = Join-Path $runDirectory "server.stderr.log"
    $clientStdoutLogPath = Join-Path $runDirectory "client.stdout.log"
    $clientStderrLogPath = Join-Path $runDirectory "client.stderr.log"
    $launcherLogPath = Join-Path $runDirectory "launcher.log"
    $rttCsvPath = Join-Path $runDirectory "rtt.csv"

    $serverConfigContent = Render-Template -TemplatePath $serverTemplatePath -Tokens @{
        "__RIO_SEND_DISPATCH_MODE__" = $runMode.DispatchMode
        "__PORT__" = $Port
        "__WORKER_THREAD_COUNT__" = $WorkerThreadCount
        "__ROOM_COUNT__" = $RoomCount
        "__ROOM_CAPACITY__" = $RoomCapacity
        "__LOG_OUTPUT_DIRECTORY__" = (Convert-ToYamlPath -Path $serverLogsDirectory)
    }
    Write-Utf8Text -Path $serverConfigPath -Content $serverConfigContent

    $clientConfigContent = Render-Template -TemplatePath $clientTemplatePath -Tokens @{
        "__PORT__" = $Port
        "__SESSION_COUNT__" = $SessionCount
        "__PAYLOAD_SIZE__" = $PayloadSize
        "__HOLD_SECONDS__" = $HoldSeconds
        "__CONNECTS_PER_SECOND__" = $ConnectsPerSecond
        "__WORKER_THREAD_COUNT__" = $WorkerThreadCount
        "__RTT_CSV_PATH__" = (Convert-ToYamlPath -Path $rttCsvPath)
    }
    Write-Utf8Text -Path $clientConfigPath -Content $clientConfigContent

    $serverProcess = $null
    $clientProcess = $null
    $serverReady = $false
    $clientTimedOut = $false
    $serverExitCode = $null
    $clientExitCode = $null
    $runStartedAt = Get-Date

    try {
        $serverProcess = Start-Process -FilePath $serverExePath `
            -ArgumentList @("--config", $serverConfigPath) `
            -WorkingDirectory (Split-Path -Parent $serverExePath) `
            -RedirectStandardOutput $serverStdoutLogPath `
            -RedirectStandardError $serverStderrLogPath `
            -PassThru

        Add-SequenceLine -Path $launcherLogPath -Line ("start={0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
        Add-SequenceLine -Path $launcherLogPath -Line ("expected_end={0}" -f (Get-Date).AddSeconds($HoldSeconds).ToString("yyyy-MM-dd HH:mm:ss"))
        Add-SequenceLine -Path $launcherLogPath -Line ("server_pid={0}" -f $serverProcess.Id)
        Add-SequenceLine -Path $launcherLogPath -Line ("server_config={0}" -f $serverConfigPath)
        Add-SequenceLine -Path $launcherLogPath -Line ("server_stdout_log={0}" -f $serverStdoutLogPath)
        Add-SequenceLine -Path $launcherLogPath -Line ("server_stderr_log={0}" -f $serverStderrLogPath)
        Add-SequenceLine -Path $launcherLogPath -Line ("rtt_csv={0}" -f $rttCsvPath)

        $serverReady = Wait-ForLogPattern -LogPath $serverStdoutLogPath -Pattern "EchoServer started." -TimeoutSeconds 30
        if (-not $serverReady) {
            throw "server ready pattern not observed within timeout."
        }

        $clientProcess = Start-Process -FilePath $clientExePath `
            -ArgumentList @("--config", $clientConfigPath) `
            -WorkingDirectory (Split-Path -Parent $clientExePath) `
            -RedirectStandardOutput $clientStdoutLogPath `
            -RedirectStandardError $clientStderrLogPath `
            -PassThru

        Add-SequenceLine -Path $launcherLogPath -Line ("client_pid={0}" -f $clientProcess.Id)
        Add-SequenceLine -Path $launcherLogPath -Line ("client_config={0}" -f $clientConfigPath)
        Add-SequenceLine -Path $launcherLogPath -Line ("client_stdout_log={0}" -f $clientStdoutLogPath)
        Add-SequenceLine -Path $launcherLogPath -Line ("client_stderr_log={0}" -f $clientStderrLogPath)

        $clientWaitResult = Wait-ProcessWithTimeout -Process $clientProcess -TimeoutSeconds ($HoldSeconds + 600)
        $clientTimedOut = $clientWaitResult.TimedOut
        $clientExitCode = $clientWaitResult.ExitCode
        if ($clientTimedOut) {
            throw "client process timed out."
        }

        Start-Sleep -Seconds 3
    }
    catch {
        Add-SequenceLine -Path $launcherLogPath -Line ("error={0}" -f $_.Exception.Message)
    }
    finally {
        $clientExitCode = if ($null -ne $clientProcess -and $clientProcess.HasExited) { $clientProcess.ExitCode } else { $clientExitCode }
        $serverExitCode = Stop-ProcessIfRunning -Process $serverProcess
    }

    $samples = Get-EchoStatsSamples -ServerStdoutLogPath $serverStdoutLogPath
    $rttSummary = Get-RttStageSummary -RttCsvPath $rttCsvPath -Stage "echo-response"
    $responsesTotal = Get-ResponsesTotal -ClientStdoutLogPath $clientStdoutLogPath
    if ($null -eq $clientExitCode -and $null -ne $responsesTotal) {
        $clientExitCode = 0
    }

    $succeeded = $serverReady -and -not $clientTimedOut -and ($clientExitCode -eq 0)
    if (-not $succeeded) {
        $failedRuns.Add($runMode.RunDirectoryName)
    }

    $runSummary = [ordered]@{
        Mode = $runMode.Mode
        Backend = "Rio"
        RioSendDispatchMode = $runMode.DispatchMode
        HoldSeconds = $HoldSeconds
        SessionCount = $SessionCount
        PayloadSize = $PayloadSize
        WorkerThreadCount = $WorkerThreadCount
        SampleCount = $samples.Count
        ResponsesTotal = $responsesTotal
        ResponseAvgPerSec = if ($null -ne $responsesTotal -and $HoldSeconds -gt 0) { [double]$responsesTotal / [double]$HoldSeconds } else { $null }
        AvgRecvTPS = Measure-SampleAverage -Samples $samples -Key "recvTPS"
        AvgSendTPS = Measure-SampleAverage -Samples $samples -Key "sendTPS"
        AvgRecvBps = Measure-SampleAverage -Samples $samples -Key "recvBps"
        AvgSendBps = Measure-SampleAverage -Samples $samples -Key "sendBps"
        AvgCpuPercent = Measure-SampleAverage -Samples $samples -Key "cpuPercent"
        EchoAvgMs = if ($null -ne $rttSummary) { Try-ParseDouble -Value $rttSummary.overall_avg_ms } else { $null }
        EchoMaxMs = if ($null -ne $rttSummary) { Try-ParseDouble -Value $rttSummary.overall_max1_ms } else { $null }
        FinalRioSendPrepareCount = Get-LastSampleValue -Samples $samples -Key "rioSendPrepareCount"
        FinalRioSendPrepareAvgNs = Get-LastSampleValue -Samples $samples -Key "rioSendPrepareAvgNs"
        FinalRioSendPrepareMaxNs = Get-LastSampleValue -Samples $samples -Key "rioSendPrepareMaxNs"
        FinalRioSendRingTouchCount = Get-LastSampleValue -Samples $samples -Key "rioSendRingTouchCount"
        FinalRioSendRingCrossThreadTouchCount = Get-LastSampleValue -Samples $samples -Key "rioSendRingCrossThreadTouchCount"
        FinalRioSendRingCrossThreadRatePercent = Get-LastSampleValue -Samples $samples -Key "rioSendRingCrossThreadRatePercent"
        FinalRioDirectSendRingLockCount = Get-LastSampleValue -Samples $samples -Key "rioDirectSendRingLockCount"
        FinalRioDirectSendRingLockWaitAvgNs = Get-LastSampleValue -Samples $samples -Key "rioDirectSendRingLockWaitAvgNs"
        FinalRioDirectSendRingLockWaitMaxNs = Get-LastSampleValue -Samples $samples -Key "rioDirectSendRingLockWaitMaxNs"
        FinalRioDirectSendRingLockHoldAvgNs = Get-LastSampleValue -Samples $samples -Key "rioDirectSendRingLockHoldAvgNs"
        FinalRioDirectSendRingLockHoldMaxNs = Get-LastSampleValue -Samples $samples -Key "rioDirectSendRingLockHoldMaxNs"
        ServerReady = $serverReady
        ClientTimedOut = $clientTimedOut
        ServerExitCode = $serverExitCode
        ClientExitCode = $clientExitCode
        StartedAt = $runStartedAt.ToString("yyyy-MM-dd HH:mm:ss")
        CompletedAt = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
        Succeeded = $succeeded
        OutputDirectory = $runDirectory
        Paths = @{
            ServerConfig = $serverConfigPath
            ClientConfig = $clientConfigPath
            ServerStdoutLog = $serverStdoutLogPath
            ServerStderrLog = $serverStderrLogPath
            ClientStdoutLog = $clientStdoutLogPath
            ClientStderrLog = $clientStderrLogPath
            RttCsv = $rttCsvPath
            LauncherLog = $launcherLogPath
        }
    }

    $runSummaryJsonPath = Join-Path $runDirectory "run-summary.json"
    Write-Utf8Text -Path $runSummaryJsonPath -Content (($runSummary | ConvertTo-Json -Depth 6))
    $summaryRows.Add((Convert-RunSummaryToCsvRow -RunSummary $runSummary)) | Out-Null
}

$summaryCsvPath = Join-Path $sequenceDirectory "summary.csv"
$summaryRows | Export-Csv -Path $summaryCsvPath -NoTypeInformation -Encoding utf8

$summaryJsonPath = Join-Path $sequenceDirectory "summary.json"
Write-Utf8Text -Path $summaryJsonPath -Content (($summaryRows | ConvertTo-Json -Depth 4))

if ($failedRuns.Count -gt 0) {
    $failedRunsPath = Join-Path $sequenceDirectory "failed-runs.txt"
    Write-Utf8Text -Path $failedRunsPath -Content (($failedRuns -join [Environment]::NewLine) + [Environment]::NewLine)
}

Write-Host ("sequence completed. output={0}" -f $sequenceDirectory)
