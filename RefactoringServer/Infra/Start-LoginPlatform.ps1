param(
    [switch]$BuildLoginServerLocally,
    [switch]$RebuildLoginServerImage
)

$ErrorActionPreference = "Stop"

$infraDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$refactoringServerDir = Split-Path -Parent $infraDir
$loginServerDir = Join-Path $refactoringServerDir "LoginServer"
$composeFile = Join-Path $infraDir "docker-compose.login-platform.yaml"
$schemaFile = Join-Path $loginServerDir "db\\schema.sql"

function Wait-ContainerHealthy {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ContainerName,

        [int]$TimeoutSeconds = 120
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $state = docker inspect --format "{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}" $ContainerName 2>$null
        if ($LASTEXITCODE -eq 0) {
            $normalized = ($state | Out-String).Trim()
            if ($normalized -eq "healthy" -or $normalized -eq "running") {
                return
            }
        }

        Start-Sleep -Seconds 2
    }

    throw "Container '$ContainerName' did not become healthy within $TimeoutSeconds seconds."
}

function Invoke-LoginServerLocalBuild {
    if (-not (Test-Path (Join-Path $loginServerDir "node_modules"))) {
        Write-Host "[LoginPlatform] node_modules가 없어 npm install을 먼저 수행합니다."
        npm install
        if ($LASTEXITCODE -ne 0) {
            throw "npm install failed."
        }
    }

    Write-Host "[LoginPlatform] LoginServer 타입 체크를 수행합니다."
    npm run check
    if ($LASTEXITCODE -ne 0) {
        throw "npm run check failed."
    }

    Write-Host "[LoginPlatform] LoginServer 로컬 빌드를 수행합니다."
    npm run build
    if ($LASTEXITCODE -ne 0) {
        throw "npm run build failed."
    }
}

Push-Location $loginServerDir
try {
    if ($BuildLoginServerLocally) {
        Invoke-LoginServerLocalBuild
    }
}
finally {
    Pop-Location
}

Write-Host "[LoginPlatform] MySQL과 Redis를 먼저 기동합니다."
docker compose -f $composeFile up -d account-mysql chat-redis
if ($LASTEXITCODE -ne 0) {
    throw "docker compose up for account-mysql/chat-redis failed."
}

Wait-ContainerHealthy -ContainerName "refactoringserver-account-mysql"
Wait-ContainerHealthy -ContainerName "refactoringserver-chat-redis"

Write-Host "[LoginPlatform] Account schema를 적용합니다."
Get-Content $schemaFile -Raw | docker exec -i refactoringserver-account-mysql mysql -uappuser -pappuser1234 accountdb
if ($LASTEXITCODE -ne 0) {
    throw "Failed to apply account schema."
}

$composeArgs = @("-f", $composeFile, "up", "-d")
if ($RebuildLoginServerImage) {
    $composeArgs += "--build"
}
$composeArgs += "login-server"

Write-Host "[LoginPlatform] LoginServer Docker 서비스를 기동합니다."
docker compose @composeArgs
if ($LASTEXITCODE -ne 0) {
    throw "docker compose up for login-server failed."
}

Wait-ContainerHealthy -ContainerName "refactoringserver-login-server"

Write-Host "[LoginPlatform] Login platform is ready."
Write-Host "  LoginServer: http://127.0.0.1:18080"
Write-Host "  Swagger UI : http://127.0.0.1:18080/docs"
