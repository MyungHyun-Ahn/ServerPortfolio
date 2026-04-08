param(
    [string]$OutputLabel = "chatting_random_128b_250_1h_4mode_poll0"
)

$packageDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$benchDirectory = Split-Path -Parent (Split-Path -Parent $packageDirectory)
$runnerPath = Join-Path $benchDirectory "Run-Benchmark.ps1"
$manifestPath = Join-Path $packageDirectory "chatting-random-128b-1h-4mode-poll0.yaml"

& $runnerPath `
    -Manifest $manifestPath `
    -OutputLabel $OutputLabel
