# LoginServer QuickStart

## 1. 목적
- 이 문서는 `RefactoringServer` 기준으로 `LoginServer + MySQL + Redis`를 가장 빠르게 띄우고 확인하는 절차를 정리한다.
- 기본 기준 경로는 `E:\Procademy\myPortfolio\ServerPortfolio\RefactoringServer`이다.

## 2. 준비물
- Docker Desktop
- PowerShell
- Node.js
- `ChattingServer` 테스트까지 할 경우 [ChattingServer.exe](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Out/ChattingServer/ChattingServer.exe)

## 3. 관련 파일
- compose: [docker-compose.login-platform.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Infra/docker-compose.login-platform.yaml)
- 시작 스크립트: [Start-LoginPlatform.ps1](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Infra/Start-LoginPlatform.ps1)
- DB schema: [schema.sql](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/db/schema.sql)
- LoginServer 프로젝트: [package.json](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/package.json)
- Swagger 문서: [openapi.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/docs/openapi.yaml)

## 4. 가장 빠른 실행
PowerShell에서 아래 명령을 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File "E:\Procademy\myPortfolio\ServerPortfolio\RefactoringServer\Infra\Start-LoginPlatform.ps1"
```

이 명령은 아래 순서로 동작한다.
- `account-mysql`, `chat-redis` 컨테이너 기동
- `schema.sql`을 MySQL에 적용
- `login-server` 컨테이너 기동
- 각 컨테이너가 `healthy` 상태가 될 때까지 대기

## 5. 로컬 빌드 후 Docker 기동
소스 변경 후 타입체크와 빌드를 먼저 돌리고 싶으면 아래처럼 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File "E:\Procademy\myPortfolio\ServerPortfolio\RefactoringServer\Infra\Start-LoginPlatform.ps1" `
  -BuildLoginServerLocally `
  -RebuildLoginServerImage
```

옵션 의미는 이렇다.
- `-BuildLoginServerLocally`: `npm run check`, `npm run build`를 호스트에서 먼저 실행
- `-RebuildLoginServerImage`: LoginServer Docker 이미지를 다시 빌드

로그인 서버 코드가 바뀌었으면 보통 두 옵션을 같이 주는 편이 안전하다.

## 6. 수동 Docker 명령
스크립트를 쓰지 않고 직접 올릴 때는 아래 명령을 사용한다.

```powershell
docker compose -f "E:\Procademy\myPortfolio\ServerPortfolio\RefactoringServer\Infra\docker-compose.login-platform.yaml" up -d
```

내릴 때는 아래 명령을 사용한다.

```powershell
docker compose -f "E:\Procademy\myPortfolio\ServerPortfolio\RefactoringServer\Infra\docker-compose.login-platform.yaml" down
```

## 7. 기동 확인
기동 후 아래 주소를 확인한다.
- Healthz: `http://127.0.0.1:18080/healthz`
- Swagger UI: `http://127.0.0.1:18080/docs`
- OpenAPI YAML: `http://127.0.0.1:18080/openapi.yaml`

`healthz`가 정상이면 `mysql`, `redis` 모두 `ok`로 나와야 한다.

예시:

```json
{
  "success": true,
  "checks": {
    "mysql": "ok",
    "redis": "ok"
  }
}
```

## 8. 컨테이너 상태 확인

```powershell
docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
```

정상일 때 기대하는 컨테이너 이름은 아래와 같다.
- `refactoringserver-account-mysql`
- `refactoringserver-chat-redis`
- `refactoringserver-login-server`

## 9. API 빠른 확인

### 9-1. 회원가입
```powershell
$body = @{
  loginId = "tester01"
  password = "test1234"
  nickname = "tester01"
} | ConvertTo-Json

Invoke-RestMethod -Method Post `
  -Uri "http://127.0.0.1:18080/auth/register" `
  -ContentType "application/json" `
  -Body $body
```

### 9-2. 로그인
```powershell
$body = @{
  loginId = "tester01"
  password = "test1234"
} | ConvertTo-Json

Invoke-RestMethod -Method Post `
  -Uri "http://127.0.0.1:18080/auth/login" `
  -ContentType "application/json" `
  -Body $body
```

정상 로그인 응답에는 아래 값이 포함된다.
- `userId`
- `nickname`
- `ticket`
- `ticketExpiresInSeconds`
- `chatServer.ip`
- `chatServer.port`

## 10. ChattingServer 연동 체크
`LoginAuth` 경로를 쓰려면 [ChattingServer.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Out/ChattingServer/Config/Server/ChattingServer.yaml)에서 아래 값이 맞아야 한다.

```yaml
LoginAuth:
  Mode: Redis
  RedisHost: 127.0.0.1
  RedisPort: 6379
  RedisDatabase: 0
  RedisKeyPrefix: "chat:ticket:"
```

그 다음 [ChattingServer.exe](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Out/ChattingServer/ChattingServer.exe)를 실행하고, `WinForms ChattingClient`에서 로그인하면 된다.

## 11. 자주 쓰는 재기동 패턴

### 11-1. LoginServer 코드만 바뀐 경우
```powershell
powershell -ExecutionPolicy Bypass -File "E:\Procademy\myPortfolio\ServerPortfolio\RefactoringServer\Infra\Start-LoginPlatform.ps1" `
  -BuildLoginServerLocally `
  -RebuildLoginServerImage
```

### 11-2. 컨테이너만 다시 띄우고 싶은 경우
```powershell
docker compose -f "E:\Procademy\myPortfolio\ServerPortfolio\RefactoringServer\Infra\docker-compose.login-platform.yaml" restart
```

### 11-3. 로그 확인
```powershell
docker logs --tail 100 refactoringserver-login-server
docker logs --tail 100 refactoringserver-account-mysql
docker logs --tail 100 refactoringserver-chat-redis
```

## 12. 트러블슈팅

### 12-1. ChattingServer에서 로그인 인증을 거부하는 경우
- `LoginServer` 이미지를 최신 소스로 다시 빌드했는지 확인한다.
- 오래된 이미지면 ticket payload 형식이 현재 `ChattingServer` 기대값과 다를 수 있다.
- 가장 안전한 재실행은 `-BuildLoginServerLocally -RebuildLoginServerImage` 옵션 조합이다.

### 12-2. `docker compose up`는 됐는데 로그인 실패하는 경우
- `http://127.0.0.1:18080/healthz`를 먼저 확인한다.
- MySQL schema가 정상 적용됐는지 본다.
- `refactoringserver-login-server` 로그를 확인한다.

### 12-3. ChattingServer가 중복 로그인 세션을 안 끊는 것처럼 보이는 경우
- 최신 [ChattingServer.exe](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Out/ChattingServer/ChattingServer.exe)로 다시 빌드/실행했는지 확인한다.
- 기존 실행 중인 `ChattingServer.exe`가 있으면 종료 후 다시 실행한다.

## 13. 참고
- 중복 로그인 계획: [003_duplicate-login-kick-previous-session-plan.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/plans/012_login-platform/003_duplicate-login-kick-previous-session-plan.md)
- LoginServer 구조 리뷰: [001_loginserver-structure-and-api-review.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/reviews/007_loginserver/001_loginserver-structure-and-api-review.md)
- 중복 로그인 프로세스 리뷰: [002_duplicate-login-process-review.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/reviews/007_loginserver/002_duplicate-login-process-review.md)
