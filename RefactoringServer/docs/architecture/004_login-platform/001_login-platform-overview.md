# Login Platform Overview

## 1. 목적
- 이 문서는 현재 `RefactoringServer`의 외부 인증 구조를 아키텍처 관점에서 정리한다.
- 범위는 `WinForms Client -> LoginServer -> MySQL/Redis -> ChattingServer LoginAuth` 흐름이다.
- 구현 상세나 API 목록은 `current`, `reviews` 문서로 내려보내고, 여기서는 책임 경계와 데이터 흐름을 고정한다.

## 2. 구성 요소

### 2-1. WinForms Client
- 사용자용 프로토타입 클라이언트다.
- `LoginServer`에는 HTTP로 로그인/회원가입 요청을 보낸다.
- `ChattingServer`에는 TCP로 접속하고 `LoginAuthRq` 패킷을 보낸다.

### 2-2. LoginServer
- [LoginServer](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer)는 `Node.js + TypeScript + Express` 기반 외부 인증 서버다.
- 계정 생성, 비밀번호 검증, chat ticket 발급을 담당한다.
- 실시간 room 상태나 세션 종료는 직접 담당하지 않는다.

### 2-3. MySQL
- 계정 저장소다.
- `loginId`, `password_hash`, `nickname` 중심으로 계정 정보를 보관한다.
- 비밀번호는 `Argon2id` 해시로 저장한다.

### 2-4. Redis
- `ChattingServer` 입장용 1회성 ticket 저장소다.
- 현재는 아래 두 종류의 key를 사용한다.
  - `chat:ticket:{ticket}`
  - `chat:active-login:{userId}`

### 2-5. ChattingServer
- 실시간 세션, lobby, room, broadcast를 담당하는 C++ 서버다.
- 외부 인증 결과는 직접 신뢰하지 않고 `LoginAuthRq`를 통해 Redis ticket를 consume한 뒤 입장을 허용한다.
- 중복 로그인 정책의 실제 집행 위치도 여기다.

### 2-6. Connector
- [Connector](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Libraries/Connector)는 `ChattingServer`와 외부 저장소 구현 사이의 경계 계층이다.
- `ChattingServer`가 Redis client 구현 세부사항을 직접 들고 있지 않게 만든다.

### 2-7. Infra
- [Infra](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Infra)는 `MySQL + Redis + LoginServer` Docker compose와 실행 스크립트를 관리한다.
- 로컬 실행, Docker 재기동, quick start는 여기 기준으로 맞춘다.

## 3. 상위 구조
```text
WinForms Client
  -> HTTP
LoginServer
  -> MySQL accounts
  -> Redis chat tickets / active login version

WinForms Client
  -> TCP + LoginAuthRq(ticket)
ChattingServer
  -> Connector
  -> Redis consume / validate
  -> Lobby / Room / Chatting
```

## 4. 로그인 흐름
1. 클라이언트가 `LoginServer`에 `loginId`, `password`를 보낸다.
2. `LoginServer`가 `MySQL`에서 계정을 조회하고 비밀번호를 검증한다.
3. 로그인 성공 시 `Redis`의 `chat:active-login:{userId}`를 증가시켜 최신 `loginVersion`을 만든다.
4. `LoginServer`가 `chat:ticket:{ticket}`에 `userId:loginVersion` 값을 TTL과 함께 기록한다.
5. 클라이언트가 `ChattingServer`에 접속한 뒤 `LoginAuthRq(ticket)`를 보낸다.
6. `ChattingServer`는 `Connector`를 통해 ticket를 consume하고 active login version과 비교한다.
7. 최신 ticket면 lobby로 입장시키고, 오래된 ticket면 거절한다.

## 5. 중복 로그인 흐름
1. 같은 계정으로 두 번째 로그인에 성공하면 `LoginServer`가 더 큰 `loginVersion`을 발급한다.
2. 두 번째 클라이언트가 `ChattingServer`에 `LoginAuthRq`를 보내면, 서버는 이 ticket가 최신 login version인지 확인한다.
3. 최신이면 `userId -> sessionId` 매핑으로 기존 세션을 찾는다.
4. `ChattingServer`가 기존 세션을 `DisconnectSession()`으로 종료한다.
5. 새 세션만 남기고 이후 room/chatting 흐름은 새 세션 기준으로 이어간다.

즉 정책은 `LoginServer에서 최신성 보장`, `ChattingServer에서 기존 세션 종료 집행`의 2단 분리다.

## 6. 책임 경계

### 6-1. LoginServer가 담당하는 것
- 회원가입
- 로그인
- 비밀번호 해시 검증
- chat ticket 발급
- 최신 login version 증가

### 6-2. ChattingServer가 담당하는 것
- ticket consume
- `LoginAuth` 인증 성공/실패 결정
- 기존 세션 강제 종료
- lobby/room/chatting 상태 소유

### 6-3. 분리 이유
- 계정 인증과 실시간 세션 상태를 분리하면 각각의 책임이 단순해진다.
- `ChattingServer`는 계정 DB를 직접 몰라도 된다.
- `LoginServer`는 실시간 세션 라우팅이나 room 상태를 몰라도 된다.

## 7. 현재 남겨둔 의도적 한계
- `legacy LoginRq/Rp` 경로는 아직 남아 있다.
- HTTPS, rate limiting, 운영용 보안 정책은 아직 2차 범위다.
- MySQL 직접 연동은 현재 `LoginServer`가 우선 담당하고, C++ 쪽은 Redis consume 중심으로 제한했다.

## 8. 관련 문서
- 현재 상태: [006_loginserver-current.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/current/006_loginserver-current.md)
- 채팅 현재 상태: [004_chatting-current.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/current/004_chatting-current.md)
- 중복 로그인 계획: [003_duplicate-login-kick-previous-session-plan.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/plans/012_login-platform/003_duplicate-login-kick-previous-session-plan.md)
- LoginServer 리뷰: [001_loginserver-structure-and-api-review.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/reviews/007_loginserver/001_loginserver-structure-and-api-review.md)
- 중복 로그인 리뷰: [002_duplicate-login-process-review.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/reviews/007_loginserver/002_duplicate-login-process-review.md)
- 실행 가이드: [LoginServerQuickStart.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/Infra/LoginServerQuickStart.md)
