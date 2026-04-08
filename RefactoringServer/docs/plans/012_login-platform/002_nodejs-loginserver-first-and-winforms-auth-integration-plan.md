# Node.js LoginServer 우선 구현 및 WinForms 인증 전환 계획

상태: 활성  
정본: 예  
최종 갱신: 2026-04-09  
범위: `Node.js LoginServer` 1차 구현과 `C# WinForms` 외부 인증 전환  
현황: `RefactoringServer/LoginServer` 1차 스캐폴드, `register/login/healthz`, `Argon2id + MySQL + Redis`, `Swagger/OpenAPI`, `Infra Docker compose` 연동 구현 완료

## 1. 목표
- 다음 작업의 시작점을 `Node.js LoginServer`로 고정한다.
- `C# WinForms ChattingClient`가 현재 `mock LoginRq` 경로에서 `HTTP 로그인 -> Redis chat ticket -> LoginAuthRq` 경로로 넘어가게 만든다.
- 이미 추가된 `ChattingServer LoginAuthRq/Rp + Redis consume` 경로를 실제 사용 흐름으로 연결한다.

## 2. 현재 기준
- `ChattingServer`는 기존 `LoginRq/Rp` 경로와 새 `LoginAuthRq/Rp` 경로를 함께 가진다.
- `Libraries/Connector`에는 `Redis ticket consume`용 `IChatTicketStore`, `FDisabledChatTicketStore`, `FRedisChatTicketStore`가 있다.
- `Infra/docker-compose.login-platform.yaml`로 `MySQL + Redis`를 로컬에서 바로 올릴 수 있다.
- `ChattingClientWinForms`는 아직 `LoginRq(userId)` 기반 mock 로그인만 사용한다.

즉 서버 쪽 `ticket consume` 준비는 됐고, 이제 필요한 것은:
1. `Node.js LoginServer`에서 계정 처리와 ticket 발급
2. `WinForms`가 그 ticket으로 `LoginAuthRq`를 보내는 전환

## 3. 왜 LoginServer부터 시작하나
- `WinForms` 전환은 결국 호출할 HTTP API 계약이 먼저 있어야 안정적으로 붙일 수 있다.
- `MySQL AccountDB`, `Redis ticket` 구조를 서버에서 먼저 고정하면 클라이언트는 그 계약만 따라가면 된다.
- 지금 `ChattingServer` 쪽 패킷과 Redis consume 경로는 이미 있으므로, 다음 병목은 C++이 아니라 로그인 플랫폼이다.

## 4. 작업 순서

### 4.1 1단계: Node.js LoginServer 스캐폴드
- 위치: `RefactoringServer/LoginServer/`
- 기본 구성:
  - `package.json`
  - `tsconfig.json`
  - `src/app.ts`
  - `src/config/env.ts`
  - `src/routes/auth.ts`
  - `src/controllers/auth-controller.ts`
  - `src/services/account-service.ts`
  - `src/services/chat-ticket-service.ts`
  - `src/db/mysql.ts`
  - `src/db/redis.ts`
  - `src/models/auth-types.ts`
  - `db/schema.sql`
- 기술 기준:
  - `TypeScript`
  - `express`
  - `mysql2/promise`
  - `redis`
  - `argon2`
  - `dotenv`

### 4.2 2단계: 계정/티켓 최소 기능 구현
- `POST /auth/register`
  - `loginId`, `password`, `nickname`
  - `MySQL`에 계정 저장
- `POST /auth/login`
  - `loginId`, `password`
  - 비밀번호 검증
  - 성공 시 `Redis`에 짧은 TTL의 `chat ticket` 저장
  - 응답으로 `ticket`, `userId`, `nickname`, `chattingServer endpoint` 반환
- `GET /healthz`
  - 프로세스 헬스체크
  - 가능하면 `MySQL`, `Redis` 연결 확인 포함

### 4.3 3단계: WinForms 인증 전환
- 로그인 UI는 더 이상 임시 `userId`를 직접 만들지 않는다.
- `WinForms`는 `Node.js LoginServer`에 HTTP 로그인 요청을 보낸다.
- 로그인 성공 시 받은 `ticket`으로 `ChattingServer` TCP 연결 후 `LoginAuthRq`를 전송한다.
- `LoginAuthRp` 성공 시에만 기존 `RoomList -> RoomChange -> Chatting` 흐름으로 들어간다.

### 4.4 4단계: prototype 경로 정리
- 초기에는 `WinForms`에 `AuthMode` 또는 `UseLoginServer` 같은 설정을 둬서
  - `PrototypeAuth`
  - `ExternalAuth`
  두 경로를 병행 유지한다.
- `ChattingDummyClient`와 기존 부하 테스트는 계속 `LoginRq` 경로를 사용한다.
- 사용자용 `WinForms`가 안정화되면 `mock login` 기본값을 끄는 방향으로 간다.

## 5. 1차 API 계약

### 5.1 회원가입
`POST /auth/register`

요청:
```json
{
  "loginId": "tester01",
  "password": "1234",
  "nickname": "tester01"
}
```

응답:
```json
{
  "success": true,
  "userId": 101,
  "nickname": "tester01"
}
```

### 5.2 로그인
`POST /auth/login`

요청:
```json
{
  "loginId": "tester01",
  "password": "1234"
}
```

응답:
```json
{
  "success": true,
  "userId": 101,
  "nickname": "tester01",
  "ticket": "9b6d5d8b-8c87-4a4d-95b1-7e4f6e1d7f2a",
  "ticketExpiresInSeconds": 60,
  "chatServer": {
    "ip": "127.0.0.1",
    "port": 19100
  }
}
```

## 6. Redis ticket 규칙
- key: `chat:ticket:{ticket}`
- value:
  - 현재 1차 구현은 `userId` 문자열 하나만 저장한다.
  - 이유는 현재 `ChattingServer`의 `Redis GETDEL consume` 구현이 value 전체를 `userId`로 해석하기 때문이다.
  - 이후 `ChattingServer` consume contract를 확장하면 JSON payload로 넓힌다.
- TTL은 초기 기준 `30~60초`
- `GETDEL` 기반 1회성 소비를 유지한다.

## 7. MySQL 1차 스키마
- `accounts`
  - `account_id BIGINT AUTO_INCREMENT PRIMARY KEY`
  - `login_id VARCHAR(64) NOT NULL UNIQUE`
  - `password_hash VARCHAR(255) NOT NULL`
  - `nickname VARCHAR(64) NOT NULL`
  - `created_at DATETIME NOT NULL`
  - `updated_at DATETIME NOT NULL`
  - `status TINYINT NOT NULL DEFAULT 1`
  - `password_hash`는 `Argon2id` 결과 문자열을 그대로 저장한다.

## 8. WinForms 변경 범위
- `AuthApiClient` 추가
- `LoginForm` 또는 `MainForm`에 HTTP 로그인/회원가입 호출 추가
- `ChattingPacketCodec`에 이미 있는 `LoginAuthRq/Rp` 상수와 직렬화 경로 연결
- `ChattingTcpClient`가 `LoginRp`와 `LoginAuthRp`를 모두 처리하도록 확장
- `PrototypeAuth`와 `ExternalAuth`를 UI 또는 config로 선택 가능하게 유지

## 9. 검증 순서
1. Docker로 `MySQL`, `Redis` 기동
2. `Node.js LoginServer` 실행
3. `POST /auth/register` 성공
4. `POST /auth/login` 성공 및 Redis ticket 저장 확인
5. `WinForms`에서 로그인 성공 후 `LoginAuthRq` 전송
6. `ChattingServer`가 `LoginAuthRp success=true` 응답
7. `RoomList`, `RoomChange`, `Chatting`, `Broadcast` 회귀 확인
8. 이미 사용한 ticket 재사용 시 로그인 실패 확인

## 10. 이번 단계에서 하지 않을 것
- JWT/OAuth
- 비밀번호 재설정
- 이메일 인증
- 관리자 기능
- C++ 쪽 `MySQL connector`
- 더미 클라이언트 외부 인증 전환

## 11. 구현 시작점
이번 다음 턴의 실제 시작 순서는 아래로 고정한다.

1. `LoginServer` 프로젝트 스캐폴드 생성
2. `.env.example`, `db/schema.sql`, 기본 Express 서버 기동
3. `MySQL/Redis` 연결 모듈 추가
4. `register/login` API 구현
5. `WinForms`에 `AuthApiClient`와 `LoginAuthRq` 전환 추가

## 12. 정리
- 다음 병목은 `ChattingServer`가 아니라 `LoginServer + WinForms 외부 인증 연결`이다.
- 따라서 다음 작업은 `Node.js LoginServer`를 먼저 세우고,
- 그다음 `C# WinForms`가 `LoginAuthRq`를 실제 사용하도록 붙이는 순서가 가장 안전하다.
