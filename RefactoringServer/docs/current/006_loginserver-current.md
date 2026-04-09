# 006 LoginServer Current

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-09  
Scope: Node.js LoginServer current state

## 1. 현재 프로젝트 구성
- 로그인 서버: [LoginServer](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer)
- API 계약 문서: [openapi.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/docs/openapi.yaml)
- 계정 스키마: [schema.sql](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/db/schema.sql)

## 2. 현재 동작 기준
- `Node.js + TypeScript + Express` 기반 로그인 서버가 1차 구현되어 있다.
- 계정 저장은 `MySQL accountdb.accounts`를 사용한다.
- 비밀번호는 `Argon2id` 해시 문자열로 저장한다.
- 로그인 성공 시 `Redis`에 `chat:ticket:{uuid}` 키로 1회성 chat ticket을 기록한다.
- 현재 `ChattingServer` consume 규약에 맞춰 Redis value는 `userId` 문자열만 저장한다.

## 3. 현재 API surface
- `POST /auth/register`
- `POST /auth/login`
- `GET /healthz`
- `GET /openapi.yaml`
- `GET /openapi.json`
- `GET /docs`
- 실패 응답은 `success=false`, `code`, `message`를 함께 반환한다.

## 4. Swagger / OpenAPI 기준
- Swagger UI 경로는 `http://127.0.0.1:18080/docs`다.
- OpenAPI YAML 원본은 [openapi.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/docs/openapi.yaml)이다.
- WinForms 연동과 외부 클라이언트 계약 확인은 이 OpenAPI 문서를 기준으로 한다.

## 5. Docker / Infra 기준
- Login platform compose는 [docker-compose.login-platform.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Infra/docker-compose.login-platform.yaml) 기준으로 관리한다.
- LoginServer Docker 이미지는 [Dockerfile](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/Dockerfile)로 빌드한다.
- 실행 스크립트는 [Start-LoginPlatform.ps1](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Infra/Start-LoginPlatform.ps1)이다.
- `-BuildLoginServerLocally` 옵션을 주면 Docker 기동 전에 호스트에서 `npm run check`, `npm run build`를 먼저 수행한다.
- `-RebuildLoginServerImage` 옵션을 주면 `docker compose up --build`로 LoginServer 이미지를 다시 만든다.

## 6. 현재 미포함 범위
- HTTPS
- rate limiting
- refresh token / session 관리
- 이메일 인증, 비밀번호 재설정
- Redis ticket payload 확장

## 7. 다음 작업
- `WinForms + ChattingServer` end-to-end 회귀를 실제 실행 기준으로 더 검증한다.
- 필요 시 Swagger 예제와 오류 응답 schema를 더 세분화한다.
- 운영 전환 전 HTTPS와 rate limit 정책을 붙인다.
