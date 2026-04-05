param(
    [int]$SessionCount = 250,
    [int]$HoldSeconds = 7200,
    [int]$RoomCount = 80,
    [int]$RoomCapacity = 4,
    [int]$RoomChangeProbabilityPercent = 90,
    [int]$RecvTimeoutMs = 15000,
    [int]$RoomListRecvTimeoutMs = 15000,
    [int]$EchoRecvTimeoutMs = 15000,
    [int]$WorkerThreadCount = 2,
    [int]$MaxSessionCount = 512,
    [int]$RttFlushIntervalSeconds = 60,
    [string[]]$ModeOrder = @("RioDirect", "RioOwnerThread", "Iocp"),
    [string]$OutputLabel = "",
    [switch]$KeepServerAliveOnFailure
)

$ErrorActionPreference = "Stop"

function Set-YamlScalarValue
{
    param(
        [string]$Content,
        [string]$SectionName,
        [string]$Key,
        [string]$Value
    )

    $escapedSection = [regex]::Escape($SectionName)
    $escapedKey = [regex]::Escape($Key)
    $pattern = "(?ms)(^${escapedSection}:\r?\n(?:^[ ]{2}.*\r?\n)*)"
    $sectionMatch = [regex]::Match($Content, $pattern)
    if (-not $sectionMatch.Success)
    {
        throw "Section not found in YAML: $SectionName"
    }

    $sectionBlock = $sectionMatch.Groups[1].Value
    $linePattern = "(?m)^  ${escapedKey}:.*$"
    if (-not [regex]::IsMatch($sectionBlock, $linePattern))
    {
        throw "Key '$Key' not found in section '$SectionName'"
    }

    $updatedSection = [regex]::Replace(
        $sectionBlock,
        $linePattern,
        ("  {0}: {1}" -f $Key, $Value),
        1)

    return $Content.Substring(0, $sectionMatch.Index) +
        $updatedSection +
        $Content.Substring($sectionMatch.Index + $sectionMatch.Length)
}

function Update-ServerConfigYaml
{
    param(
        [string]$TemplateContent,
        [string]$Backend,
        [string]$RioSendDispatchMode,
        [string]$LogOutputDirectory,
        [int]$WorkerThreadCount,
        [int]$MaxSessionCount,
        [int]$RoomCount,
        [int]$RoomCapacity
    )

    $yaml = $TemplateContent
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoServer" -Key "Backend" -Value $Backend
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoServer" -Key "RioSendDispatchMode" -Value $RioSendDispatchMode
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoServer" -Key "WorkerThreadCount" -Value $WorkerThreadCount
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoServer" -Key "MaxSessionCount" -Value $MaxSessionCount
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoServer" -Key "RoomCount" -Value $RoomCount
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoServer" -Key "RoomCapacity" -Value $RoomCapacity
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoServer" -Key "LogOutputDirectory" -Value ('"{0}"' -f $LogOutputDirectory.Replace('\', '/'))
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "Debug" -Key "Headless" -Value "true"
    return $yaml
}

function Update-ClientConfigYaml
{
    param(
        [string]$TemplateContent,
        [int]$SessionCount,
        [int]$HoldSeconds,
        [int]$RoomChangeProbabilityPercent,
        [int]$RecvTimeoutMs,
        [int]$RoomListRecvTimeoutMs,
        [int]$EchoRecvTimeoutMs,
        [int]$RttFlushIntervalSeconds,
        [string]$RttCsvPath
    )

    $yaml = $TemplateContent
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoClient" -Key "SessionCount" -Value $SessionCount
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoClient" -Key "HoldSeconds" -Value $HoldSeconds
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "EchoClient" -Key "RoomChangeProbabilityPercent" -Value $RoomChangeProbabilityPercent
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "Debug" -Key "RecvTimeoutMs" -Value $RecvTimeoutMs
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "Debug" -Key "RoomListRecvTimeoutMs" -Value $RoomListRecvTimeoutMs
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "Debug" -Key "EchoRecvTimeoutMs" -Value $EchoRecvTimeoutMs
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "Debug" -Key "RttFlushIntervalSeconds" -Value $RttFlushIntervalSeconds
    $yaml = Set-YamlScalarValue -Content $yaml -SectionName "Debug" -Key "RttCsvPath" -Value ("'{0}'" -f $RttCsvPath.Replace('\', '/'))
    return $yaml
}

function Get-AverageFromServerLog
{
    param(
        [string]$LogPath,
        [string]$MetricName
    )

    if (-not (Test-Path $LogPath))
    {
        return $null
    }

    $pattern = "{0}=(?<value>[0-9]+(?:\.[0-9]+)?)" -f [regex]::Escape($MetricName)
    $values = New-Object System.Collections.Generic.List[double]
    foreach ($line in Get-Content -Path $LogPath)
    {
        $match = [regex]::Match($line, $pattern)
        if ($match.Success)
        {
            $values.Add([double]$match.Groups["value"].Value)
        }
    }

    if ($values.Count -eq 0)
    {
        return $null
    }

    return ($values | Measure-Object -Average).Average
}

function Get-LatestRttRowByStage
{
    param(
        [string]$CsvPath,
        [string]$StageName
    )

    if (-not (Test-Path $CsvPath))
    {
        return $null
    }

    $rows = Import-Csv -Path $CsvPath
    $stageRows = @($rows | Where-Object { $_.stage -eq $StageName })
    if ($stageRows.Count -eq 0)
    {
        return $null
    }

    return $stageRows[-1]
}

function Get-ModeDescriptor
{
    param([string]$ModeName)

    switch ($ModeName)
    {
        "RioDirect"
        {
            return @{
                Name = "RioDirect"
                Backend = "Rio"
                RioSendDispatchMode = "Direct"
                DirectoryName = "rio_direct"
            }
        }
        "RioOwnerThread"
        {
            return @{
                Name = "RioOwnerThread"
                Backend = "Rio"
                RioSendDispatchMode = "OwnerThread"
                DirectoryName = "rio_owner"
            }
        }
        "Iocp"
        {
            return @{
                Name = "Iocp"
                Backend = "Iocp"
                RioSendDispatchMode = "Direct"
                DirectoryName = "iocp"
            }
        }
        default
        {
            throw "Unsupported mode name: $ModeName"
        }
    }
}

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $scriptDirectory
$outDirectory = Join-Path $root "Out"
$echoServer = Join-Path $outDirectory "EchoServer.exe"
$echoClient = Join-Path $outDirectory "EchoClient.exe"
$serverTemplatePath = Join-Path $root "Config\\Server\\EchoServer.yaml"
$clientTemplatePath = Join-Path $root "Config\\Client\\EchoClient.yaml"

if (-not (Test-Path $echoServer)) { throw "EchoServer.exe not found: $echoServer" }
if (-not (Test-Path $echoClient)) { throw "EchoClient.exe not found: $echoClient" }
if (-not (Test-Path $serverTemplatePath)) { throw "Server config template not found: $serverTemplatePath" }
if (-not (Test-Path $clientTemplatePath)) { throw "Client config template not found: $clientTemplatePath" }

$serverTemplateContent = Get-Content -Path $serverTemplatePath -Raw -Encoding utf8
$clientTemplateContent = Get-Content -Path $clientTemplatePath -Raw -Encoding utf8

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$labelSuffix = if ([string]::IsNullOrWhiteSpace($OutputLabel)) { "" } else { "_$OutputLabel" }
$sequenceDirectory = Join-Path $outDirectory ("dispatch_ab_2h_{0}{1}" -f $timestamp, $labelSuffix)
New-Item -ItemType Directory -Force -Path $sequenceDirectory | Out-Null

$summaryRows = New-Object System.Collections.Generic.List[psobject]
$sequenceLog = Join-Path $sequenceDirectory "sequence.log"

@(
    "start=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    "session_count=$SessionCount",
    "hold_seconds=$HoldSeconds",
    "room_count=$RoomCount",
    "room_capacity=$RoomCapacity",
    "room_change_probability_percent=$RoomChangeProbabilityPercent",
    "recv_timeout_ms=$RecvTimeoutMs",
    "room_list_recv_timeout_ms=$RoomListRecvTimeoutMs",
    "echo_recv_timeout_ms=$EchoRecvTimeoutMs",
    "worker_thread_count=$WorkerThreadCount",
    "max_session_count=$MaxSessionCount",
    "mode_order=$($ModeOrder -join ',')"
) | Set-Content -Encoding utf8 $sequenceLog

foreach ($modeName in $ModeOrder)
{
    $mode = Get-ModeDescriptor -ModeName $modeName
    $runDirectory = Join-Path $sequenceDirectory $mode.DirectoryName
    New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null

    $serverConfigPath = Join-Path $runDirectory ("EchoServer.{0}.yaml" -f $mode.Name)
    $clientConfigPath = Join-Path $runDirectory ("EchoClient.{0}.yaml" -f $mode.Name)
    $serverLog = Join-Path $runDirectory "server.log"
    $serverErr = Join-Path $runDirectory "server.err.log"
    $clientLog = Join-Path $runDirectory "client.log"
    $clientErr = Join-Path $runDirectory "client.err.log"
    $rttCsv = Join-Path $runDirectory "rtt.csv"
    $launcherLog = Join-Path $runDirectory "launcher.log"

    $serverYaml = Update-ServerConfigYaml `
        -TemplateContent $serverTemplateContent `
        -Backend $mode.Backend `
        -RioSendDispatchMode $mode.RioSendDispatchMode `
        -LogOutputDirectory $runDirectory `
        -WorkerThreadCount $WorkerThreadCount `
        -MaxSessionCount $MaxSessionCount `
        -RoomCount $RoomCount `
        -RoomCapacity $RoomCapacity
    $serverYaml | Set-Content -Encoding utf8 $serverConfigPath

    $clientYaml = Update-ClientConfigYaml `
        -TemplateContent $clientTemplateContent `
        -SessionCount $SessionCount `
        -HoldSeconds $HoldSeconds `
        -RoomChangeProbabilityPercent $RoomChangeProbabilityPercent `
        -RecvTimeoutMs $RecvTimeoutMs `
        -RoomListRecvTimeoutMs $RoomListRecvTimeoutMs `
        -EchoRecvTimeoutMs $EchoRecvTimeoutMs `
        -RttFlushIntervalSeconds $RttFlushIntervalSeconds `
        -RttCsvPath $rttCsv
    $clientYaml | Set-Content -Encoding utf8 $clientConfigPath

    Get-Process EchoServer, EchoClient -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 1

    $start = Get-Date
    $expectedEnd = $start.AddSeconds($HoldSeconds)
    $server = Start-Process -FilePath $echoServer -ArgumentList @("--config", $serverConfigPath) -RedirectStandardOutput $serverLog -RedirectStandardError $serverErr -PassThru
    Start-Sleep -Seconds 2
    $client = Start-Process -FilePath $echoClient -ArgumentList @("--config", $clientConfigPath) -RedirectStandardOutput $clientLog -RedirectStandardError $clientErr -PassThru

    @(
        "start=$($start.ToString('yyyy-MM-dd HH:mm:ss'))",
        "expected_end=$($expectedEnd.ToString('yyyy-MM-dd HH:mm:ss'))",
        "server_pid=$($server.Id)",
        "client_pid=$($client.Id)",
        "server_config=$serverConfigPath",
        "client_config=$clientConfigPath",
        "server_log=$serverLog",
        "client_log=$clientLog",
        "rtt_csv=$rttCsv"
    ) | Set-Content -Encoding utf8 $launcherLog

    $waitTimeoutSeconds = [Math]::Max($HoldSeconds + 180, 600)
    $client | Wait-Process -Timeout $waitTimeoutSeconds
    $client.Refresh()
    $clientExitCode = $client.ExitCode
    Start-Sleep -Seconds 5

    if ($clientExitCode -ne 0)
    {
        if (-not $KeepServerAliveOnFailure -and -not $server.HasExited)
        {
            Stop-Process -Id $server.Id -Force
        }

        throw ("Benchmark mode '{0}' failed with client exit code {1}. See: {2}" -f $mode.Name, $clientExitCode, $runDirectory)
    }

    if (-not $server.HasExited)
    {
        Stop-Process -Id $server.Id -Force
    }

    $clientSuccessLine = Select-String -Path $clientLog -Pattern "echo validation succeeded" | Select-Object -Last 1
    $responsesTotal = $null
    if ($null -ne $clientSuccessLine)
    {
        $responsesMatch = [regex]::Match($clientSuccessLine.Line, "responses=(?<value>[0-9]+)")
        if ($responsesMatch.Success)
        {
            $responsesTotal = [int64]$responsesMatch.Groups["value"].Value
        }
    }

    $echoRow = Get-LatestRttRowByStage -CsvPath $rttCsv -StageName "echo-response"
    $roomChangeListRow = Get-LatestRttRowByStage -CsvPath $rttCsv -StageName "room-change-list"
    $roomChangeRow = Get-LatestRttRowByStage -CsvPath $rttCsv -StageName "room-change"

    $summaryRows.Add([pscustomobject]@{
        Mode = $mode.Name
        Backend = $mode.Backend
        RioSendDispatchMode = $mode.RioSendDispatchMode
        HoldSeconds = $HoldSeconds
        ResponsesTotal = $responsesTotal
        AvgRecvTPS = Get-AverageFromServerLog -LogPath $serverLog -MetricName "recvTPS"
        AvgSendTPS = Get-AverageFromServerLog -LogPath $serverLog -MetricName "sendTPS"
        AvgRecvBps = Get-AverageFromServerLog -LogPath $serverLog -MetricName "recvBps"
        AvgSendBps = Get-AverageFromServerLog -LogPath $serverLog -MetricName "sendBps"
        AvgCpuPercent = Get-AverageFromServerLog -LogPath $serverLog -MetricName "cpuPercent"
        EchoAvgMs = if ($null -ne $echoRow) { [double]$echoRow.overall_avg_ms } else { $null }
        EchoMaxMs = if ($null -ne $echoRow) { [double]$echoRow.overall_max1_ms } else { $null }
        RoomChangeListAvgMs = if ($null -ne $roomChangeListRow) { [double]$roomChangeListRow.overall_avg_ms } else { $null }
        RoomChangeListMaxMs = if ($null -ne $roomChangeListRow) { [double]$roomChangeListRow.overall_max1_ms } else { $null }
        RoomChangeAvgMs = if ($null -ne $roomChangeRow) { [double]$roomChangeRow.overall_avg_ms } else { $null }
        RoomChangeMaxMs = if ($null -ne $roomChangeRow) { [double]$roomChangeRow.overall_max1_ms } else { $null }
        OutputDirectory = $runDirectory
    })
}

$summaryCsv = Join-Path $sequenceDirectory "summary.csv"
$summaryRows | Export-Csv -NoTypeInformation -Encoding utf8 $summaryCsv

Write-Host "Sequential benchmark completed."
Write-Host "sequence directory: $sequenceDirectory"
Write-Host "summary csv: $summaryCsv"
$summaryRows | Format-Table -AutoSize
