# LoginServer Structure And API Review

## 1. 문서 목적
- [LoginServer](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer) 1차 구현의 구조와 책임 분리를 빠르게 파악하기 위한 리뷰 문서다.
- `Node.js LoginServer`가 현재 어떤 외부 의존성을 가지고, 어떤 API를 제공하며, 각 API가 어떤 내부 경로를 타는지 정리한다.
- 이후 `WinForms -> LoginServer -> Redis ticket -> ChattingServer LoginAuthRq` 경로를 확장할 때 기준 문서로 사용한다.

## 2. 리뷰 대상 범위
- 엔트리포인트:
  - [app.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/app.ts)
- 환경설정:
  - [env.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/config/env.ts)
- 라우터 / 컨트롤러:
  - [auth.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/routes/auth.ts)
  - [auth-controller.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/controllers/auth-controller.ts)
- 서비스:
  - [account-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/account-service.ts)
  - [chat-ticket-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/chat-ticket-service.ts)
- DB / 캐시 연결:
  - [mysql.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/db/mysql.ts)
  - [redis.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/db/redis.ts)
- 계약 / 스키마:
  - [auth-types.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/models/auth-types.ts)
  - [schema.sql](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/db/schema.sql)
  - [openapi.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/docs/openapi.yaml)

## 3. 현재 구조 요약
- 서버 프레임은 `Express` 하나로 단순하게 유지한다.
- 라우터는 `/auth` 하위 엔드포인트 연결만 담당하고, 실제 응답 조립은 `AuthController`가 맡는다.
- `AccountService`는 계정 생성과 인증 검증을 담당한다.
- `ChatTicketService`는 로그인 성공 후 Redis에 채팅 입장용 1회성 ticket을 기록한다.
- `mysql.ts`, `redis.ts`는 각각 lazy singleton 형태로 연결 객체를 보관한다.
- `env.ts`는 모든 런타임 설정과 기본값을 단일 위치에서 읽는다.

## 4. 종속성 정리

### 4-1. 외부 런타임 종속성
- `MySQL`
  - 계정 저장소 역할
  - `accounts` 테이블 사용
- `Redis`
  - `chat:ticket:{uuid}` 형태의 ticket 저장소 역할
- `ChattingServer`
  - LoginServer가 직접 호출하지는 않지만, 로그인 응답에 `chatServer.ip`, `chatServer.port`를 포함해 클라이언트를 넘긴다.
- `WinForms Client`
  - `register/login` API의 직접 호출자

### 4-2. npm 패키지 종속성
- [package.json](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/package.json)
- 주요 런타임 패키지:
  - `express`
  - `argon2`
  - `mysql2`
  - `redis`
  - `dotenv`
  - `swagger-ui-express`
  - `js-yaml`
- 주요 개발 패키지:
  - `typescript`
  - `tsx`
  - `@types/node`
  - `@types/express`

### 4-3. 내부 모듈 의존 방향
- `app.ts`
  - `env.ts`
  - `AuthController`
  - `mysql.ts`
  - `redis.ts`
  - `auth.ts`
  - `openapi.ts`
- `AuthController`
  - `AccountService`
  - `ChatTicketService`
  - `mysql.ts`, `redis.ts` 검사용 함수
- `AccountService`
  - `env.ts`
  - `mysql.ts`
  - `argon2`
- `ChatTicketService`
  - `env.ts`
  - `redis.ts`
  - `crypto.randomUUID`

### 4-4. 런타임 의존 흐름
1. `app.ts` 부트스트랩
2. `verifyMySqlConnection()`, `verifyRedisConnection()` 선검증
3. `Express app` 생성
4. `/auth`, `/docs`, `/openapi.*`, `/healthz` 등록
5. 종료 시 `closeRedisClient()`, `closeMySqlPool()` 호출

## 5. 제공 API 목록

| Method | Path | 역할 | 주요 의존성 |
| --- | --- | --- | --- |
| `POST` | `/auth/register` | 계정 생성 | `AccountService`, `MySQL`, `Argon2id` |
| `POST` | `/auth/login` | 계정 인증 후 chat ticket 발급 | `AccountService`, `ChatTicketService`, `MySQL`, `Redis` |
| `GET` | `/healthz` | MySQL/Redis 상태 점검 | `mysql.ts`, `redis.ts` |
| `GET` | `/docs` | Swagger UI | `swagger-ui-express`, `openapi.yaml` |
| `GET` | `/openapi.yaml` | OpenAPI 원본 노출 | `openapi.ts` |
| `GET` | `/openapi.json` | OpenAPI JSON 노출 | `openapi.ts` |

## 6. API별 처리 흐름

### 6-1. `POST /auth/register`
1. [auth.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/routes/auth.ts)에서 요청을 `AuthController.register()`로 전달한다.
2. [auth-controller.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/controllers/auth-controller.ts)에서 [account-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/account-service.ts)의 `registerAccount()`를 호출한다.
3. `registerAccount()`는 `loginId`, `password`, `nickname`을 `normalizeRequiredString()`으로 검증한다.
4. `createPasswordHash()`가 `Argon2id`로 비밀번호 해시를 생성한다.
5. [mysql.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/db/mysql.ts)의 pool을 통해 `accounts` 테이블에 `INSERT`를 수행한다.
6. 중복 키면 `LOGIN_ID_ALREADY_EXISTS`로 변환한다.
7. 성공 시 `201 Created`와 `userId`, `nickname`을 반환한다.

### 6-2. `POST /auth/login`
1. [auth.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/routes/auth.ts)에서 요청을 `AuthController.login()`으로 전달한다.
2. `AuthController.login()`이 `AccountService.authenticate()`를 호출한다.
3. `authenticate()`는 입력값 검증 후 `login_id`로 `SELECT ... LIMIT 1` 조회를 수행한다.
4. 계정이 없으면 `LOGIN_ID_NOT_FOUND`, `status != 1`이면 `ACCOUNT_NOT_ACTIVE`를 반환한다.
5. `argon2.verify()`로 저장된 `password_hash`와 입력 비밀번호를 비교한다.
6. 비밀번호가 틀리면 `PASSWORD_MISMATCH`를 반환한다.
7. 인증 성공 시 `ChatTicketService.issueTicket()`을 호출한다.
8. `issueTicket()`는 `randomUUID()`로 ticket을 만들고, Redis에 `chat:ticket:{ticket}` 키로 `userId` 문자열을 TTL과 함께 저장한다.
9. 성공 응답에 `ticket`, `ticketExpiresInSeconds`, `chatServer.ip`, `chatServer.port`를 넣어 반환한다.

### 6-3. `GET /healthz`
1. `AuthController.healthz()`가 `verifyMySqlConnection()`과 `verifyRedisConnection()`을 각각 호출한다.
2. 둘 다 성공하면 `200 OK`와 `success=true`를 반환한다.
3. 하나라도 실패하면 `503`과 `errors[]`를 반환한다.
4. 이 API는 `WinForms`보다 Docker/운영 확인용 성격이 더 강하다.

### 6-4. `GET /docs`, `/openapi.yaml`, `/openapi.json`
1. [app.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/app.ts)에서 직접 라우트를 등록한다.
2. [openapi.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/utils/openapi.ts)가 YAML 파일을 읽는다.
3. `/docs`는 Swagger UI, `/openapi.*`는 계약 원본 제공 역할을 한다.

## 7. 데이터 저장 구조

### 7-1. MySQL `accounts`
- [schema.sql](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/db/schema.sql)
- 주요 컬럼:
  - `account_id`
  - `login_id`
  - `password_hash`
  - `nickname`
  - `status`
  - `created_at`
  - `updated_at`

### 7-2. Redis ticket
- key:
  - `chat:ticket:{uuid}`
- value:
  - 현재는 `userId` 문자열 하나만 저장
- TTL:
  - 기본 `60초`

## 8. 구조상 장점
- HTTP 진입점, 계정 검증, ticket 발급 책임이 비교적 명확하게 나뉘어 있다.
- 비밀번호 저장이 `SHA-256` 같은 단순 해시가 아니라 `Argon2id`로 고정돼 있다.
- `MySQL`과 `Redis` 연결이 lazy singleton이라 초기 코드가 단순하다.
- `OpenAPI + Swagger UI`가 같이 있어 WinForms 계약 확인이 쉽다.
- 로그인 성공 응답이 곧바로 `ChattingServer` 접속 정보와 ticket을 포함하므로 클라이언트 흐름이 짧다.

## 9. 현재 설계 리스크와 후속 후보

### 9-1. Redis ticket payload가 너무 단순함
- 현재 [chat-ticket-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/chat-ticket-service.ts)는 value에 `userId` 문자열만 저장한다.
- 이는 현재 `ChattingServer` consume 구현에 맞춘 선택이지만, 이후 `nickname`, `issuedAt`, `issuer`, `nonce` 같은 메타데이터를 담기 어렵다.

### 9-2. API 에러 응답 일관성은 일부만 보장됨
- `AuthController.writeError()`를 타는 경로는 `code/message`를 보장한다.
- 반면 `404 Route not found` 응답은 같은 형식이지만 세부 에러 코드가 없다.
- 이후 API 수가 늘어나면 공통 error middleware로 정리하는 편이 낫다.

### 9-3. 인증 보호 장치가 아직 최소 수준임
- `HTTPS`
- `rate limiting`
- 로그인 실패 횟수 제한
- refresh/session 관리
- 비밀번호 재설정
- 위 항목은 아직 범위 밖이다.

### 9-4. 계정 상태 모델이 단순함
- `status`는 현재 숫자값 `1`만 활성으로 사용한다.
- `suspended`, `deleted`, `email-unverified` 같은 상태 확장이 필요하면 enum 의미를 문서화해야 한다.

## 10. 코드 읽기 순서 추천
1. [app.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/app.ts)
2. [auth.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/routes/auth.ts)
3. [auth-controller.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/controllers/auth-controller.ts)
4. [account-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/account-service.ts)
5. [chat-ticket-service.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/services/chat-ticket-service.ts)
6. [mysql.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/db/mysql.ts)
7. [redis.ts](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/src/db/redis.ts)
8. [openapi.yaml](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/LoginServer/docs/openapi.yaml)

## 11. 요약
- `LoginServer`는 현재 `계정 검증 + 채팅 입장 ticket 발급`에 집중한 좁은 책임의 서비스다.
- `MySQL`은 계정 영속화, `Redis`는 짧은 TTL의 1회성 ticket 저장소로 역할이 분리돼 있다.
- 구조는 단순하고 확장 여지는 충분하지만, ticket payload 확장성과 공통 에러 처리 정리는 다음 단계에서 손볼 가치가 있다.
