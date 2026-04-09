# Node.js LoginServer + WinForms ChattingClient 프로토타입 계획

## 1. 목적
- 현재 `ChattingServer`에 바로 붙을 수 있는 `C# WinForms` 프로토타입 클라이언트를 먼저 만든다.
- 프로토타입 단계에서는 로그인/회원가입 UI를 빠르게 열고, `채팅방 목록 -> 입장 -> 채팅`까지 실제 서버와 연결해 확인한다.
- 이후 `Node.js LoginServer + Redis ticket exchange + MySQL AccountDB` 구조로 자연스럽게 넘어갈 수 있게 경계를 먼저 고정한다.

## 2. 왜 이 방향이 맞는가
- 인증/계정 관리는 `Node.js + MySQL`로 분리하는 편이 `C++ ChattingServer`의 책임을 가볍게 유지하기 쉽다.
- 채팅 서버는 세션, 룸, fan-out, RTT, backend 비교에 집중하고, 계정/비밀번호/회원가입은 외부 인증 서버가 맡는 구성이 더 깔끔하다.
- `Redis`를 짧은 수명의 인증 ticket 교환 버스로 두면, 로그인 서버와 채팅 서버를 느슨하게 연결하면서도 1회성 인증 흐름을 만들 수 있다.
- 다만 지금은 외부 인증보다 `WinForms 프로토타입`이 더 급하므로, 인증 구조 때문에 UI/기능 검증이 막히지 않도록 2단계로 나눈다.

## 3. 현재 기준과 제약
- 현재 [`Packet/Login/Login.yaml`](D:\Project\ServerPortfolio\RefactoringServer\Packet\Login\Login.yaml)의 `LoginRq`는 `uint32 userId`만 가진다.
- 현재 [`FAuthContent.cpp`](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingServer\Contents\Auth\FAuthContent.cpp) 기준 로그인 성공 조건은 사실상 `userId != 0`이다.
- 즉, 현재 서버는 `아이디/비밀번호`, `회원가입`, `토큰`, `세션 ticket`을 전혀 모른다.
- 현재 [`ChattingClientWinForms`](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingClientWinForms) 1차 프로토타입은 추가됐다.
- 이 프로토타입은 서버와 직접 TCP 연결하고, `Login/RoomList/RoomChange/Chatting/Broadcast` 최소 subset만 구현한다.
- 따라서 “아무거나 입력해도 통과되는 로그인 UI”와 “나중에 외부 인증으로 대체 가능한 구조”를 같이 설계해야 한다.

## 4. 핵심 결정
- 외부 인증 서버는 `Node.js LoginServer`로 분리한다.
- 계정 영속 저장소는 `MySQL AccountDB`를 사용한다.
- 로그인 성공 후 채팅 서버로 넘기는 인증 매개체는 `Redis`의 짧은 TTL을 가진 `1회성 chat ticket`으로 둔다.
- `WinForms` 1차 프로토타입에서는 실제 계정 검증 없이 어떤 입력이 들어와도 로그인/회원가입이 성공하는 `PrototypeAuth` 모드로 간다.
- 1차 프로토타입에서는 기존 `LoginRq/Rp`를 그대로 사용하고, 2차에서 `ticket 기반 로그인 패킷`을 추가하는 방식으로 확장한다.
- 기존 더미/벤치 경로를 깨지 않도록, `legacy insecure login`과 `external ticket login`은 한동안 병행 가능하게 둔다.

## 5. 단계별 목표

### 5.1 1차 목표: 급한 WinForms 프로토타입
- `C# WinForms` 클라이언트 프로젝트 생성
- 로그인 화면
- 회원가입 화면
- 채팅방 목록 조회
- 채팅방 선택 입장
- 채팅 송수신
- 현재 `ChattingServer`와 실제 연결

현재 상태:
- `ChattingClientWinForms` 프로젝트 생성 완료
- mock 로그인 / mock 회원가입 UI 추가
- 룸 목록 조회 / 룸 입장 / 채팅 송수신 UI 추가
- 서버 wire format에 맞춘 최소 `PacketCodec + PacketCipher + PacketFramer` 구현 추가

### 5.2 2차 목표: 인증 경계 정리
- `Node.js LoginServer` 초안 생성
- `MySQL` 계정 테이블 추가
- `Redis` chat ticket 발급/소비 경로 추가
- `ChattingServer`에 `ticket login` 경로 추가

### 5.3 3차 목표: 프로토타입에서 실제 인증으로 전환
- WinForms 로그인/회원가입 UI가 `Node.js LoginServer` HTTP API를 호출
- 로그인 성공 후 받은 chat ticket으로 `ChattingServer` 입장
- `PrototypeAuth` 경로는 config로 끄거나 개발 전용으로 한정

## 6. 1차 WinForms 프로토타입 범위

### 6.1 포함
- `Login` 탭
- `Register` 탭
- 서버 접속 상태 표시
- `RoomListRq/Rp`
- `RoomChangeRq/Rp`
- `ChattingRq/Rp`
- `Broadcast` 수신
- 최소 채팅 로그 UI

### 6.2 비포함
- 비밀번호 해시/실계정 검증
- 친구/귓속말/공지
- 프로필 편집
- 운영자 기능
- JWT/OAuth 같은 외부 인증 체계

### 6.3 1차 로그인/회원가입 동작 규칙
- 로그인 화면은 `아이디`, `비밀번호` 입력창을 둔다.
- 회원가입 화면은 `아이디`, `비밀번호`, `닉네임` 정도만 둔다.
- 그러나 1차에서는 어떤 입력을 넣어도 성공 처리한다.
- 현재 서버는 `userId != 0`만 통과하므로, WinForms 클라이언트가 입력값을 바탕으로 `0이 아닌 임시 userId`를 로컬 생성해 `LoginRq`에 넣는다.
- 입력이 비어 있어도 프로토타입에서는 임시 `userId`를 생성해 통과시킨다.
- 회원가입은 1차에서는 `UI/흐름 검증용 mock success`로 처리하고, 2차부터 `Node.js + MySQL`로 실제 저장한다.

## 7. 1차 UI 구조

### 7.1 화면 구성
- `Login/Register Form`
  - 로그인 탭
  - 회원가입 탭
  - 서버 주소/포트 입력
- `Lobby/Room Form`
  - 접속 상태
  - 내 `userId` 또는 표시 이름
  - 채팅방 목록
  - 입장 버튼
- `Chat Form` 또는 같은 폼의 채팅 패널
  - 현재 room 정보
  - 메시지 로그
  - 입력창
  - 전송 버튼
  - 방 나가기 또는 방 변경 버튼

### 7.2 표시 규칙
- 1차에서는 `displayName`보다 `userId` 기반 표시를 기본으로 한다.
- `Broadcast.senderUserId`만으로도 누가 보냈는지 구분 가능하게 UI를 단순하게 유지한다.
- 닉네임 표시와 프로필 동기화는 2차 이후로 미룬다.

## 8. 1차 네트워크 / 패킷 전략
- `WinForms` 클라이언트는 현재 `ChattingServer`의 바이너리 프로토콜 최소 subset만 구현한다.
- 구현 대상 packet:
  - `LoginRq / LoginRp`
  - `RoomListRq / RoomListRp`
  - `RoomChangeRq / RoomChangeRp`
  - `ChattingRq / ChattingRp`
  - `Broadcast`
- payload text는 1차에서 `UTF-8 bytes`로 보낸다.
- `clientMessageId`, `sentTick`은 클라이언트 내부 카운터와 로컬 tick으로 채운다.
- 현재 cipher/framing 규칙을 따라야 하므로, `C# PacketCodec + PacketFramer + PacketCipher` 최소 구현이 필요하다.

## 9. 프로젝트 구조 제안

### 9.1 WinForms
- `RefactoringServer/Chatting/ChattingClientWinForms/`
  - `ChattingClientWinForms.csproj`
  - `Program.cs`
  - `Forms/LoginForm.cs`
  - `Forms/MainChatForm.cs`
  - `Networking/ChatTcpClient.cs`
  - `Networking/PacketCodec.cs`
  - `Networking/PacketFramer.cs`
  - `Models/*.cs`

### 9.2 Node.js LoginServer
- `RefactoringServer/LoginServer/`
  - `package.json`
  - `src/app.ts`
  - `src/routes/auth.ts`
  - `src/services/account-service.ts`
  - `src/services/chat-ticket-service.ts`
  - `src/db/mysql.ts`
  - `src/db/redis.ts`
  - `db/schema.sql`

## 10. 프로토타입 데이터 흐름

### 10.1 1차 PrototypeAuth 흐름
1. 사용자가 WinForms 로그인 화면에서 아무 값이나 입력
2. 클라이언트가 로컬 규칙으로 `temporaryUserId != 0` 생성
3. 클라이언트가 `ChattingServer`에 TCP 연결
4. 현재 `LoginRq(userId)` 전송
5. 서버가 성공 응답 시 `Lobby`로 이동
6. 클라이언트가 `RoomListRq -> RoomChangeRq -> ChattingRq` 순으로 진행

### 10.2 최종 ExternalAuth 흐름
1. 사용자가 WinForms 로그인 화면에서 계정 입력
2. WinForms가 `Node.js LoginServer`의 `/auth/login` 호출
3. `Node.js LoginServer`가 `MySQL AccountDB`에서 계정 확인
4. 성공 시 `Redis`에 짧은 TTL의 `chat ticket` 저장
5. WinForms가 `chat ticket`과 채팅 서버 endpoint를 받음
6. WinForms가 `ChattingServer`에 TCP 연결
7. `ChattingServer`가 `ticket login packet`을 받고 `Redis`에서 ticket을 소비
8. ticket이 유효하면 세션을 `Lobby`로 이동
9. 이후 룸 선택/입장/채팅 흐름은 동일

## 11. Node.js LoginServer 설계 초안

### 11.1 책임
- 회원가입
- 로그인
- 계정 조회 최소 기능
- chat ticket 발급
- 계정/인증 관련 에러 응답

### 11.2 비책임
- 채팅방 목록 제공
- 룸 입장
- 채팅 송수신
- 채팅 세션 유지

### 11.3 최소 API
- `POST /auth/register`
- `POST /auth/login`
- `POST /auth/issue-chat-ticket`
- `GET /healthz`

초기 단순화:
- `POST /auth/login` 성공 시 바로 chat ticket까지 같이 내려줘도 된다.

## 12. MySQL AccountDB 초안

### 12.1 최소 테이블
- `accounts`
  - `account_id BIGINT PK`
  - `login_id VARCHAR(...) UNIQUE`
  - `password_hash VARCHAR(...)`
  - `display_name VARCHAR(...)`
  - `created_at`
  - `updated_at`
  - `status`

### 12.2 1차 규칙
- 비밀번호는 실제 연동 단계에서만 저장한다.
- 실제 연동 단계에서는 plain text 저장을 금지하고 hash 저장으로 제한한다.
- 프로토타입 단계에서는 `register/login`이 mock이므로 DB 저장을 생략할 수 있다.

## 13. Redis ticket exchange 초안

### 13.1 key 형태
- `chat:ticket:{ticket}`

### 13.2 value 최소 정보
- `accountId`
- `displayName`
- `issuedAt`
- `expiresAt`
- 필요 시 `clientVersion`

### 13.3 규칙
- TTL은 짧게 둔다. 초기 기준은 `30~60초`.
- ticket은 `1회성`으로 사용한다.
- `ChattingServer`가 인증 성공 시 즉시 소비해서 재사용을 막는다.
- expired / missing / already-consumed ticket은 로그인 실패로 처리한다.

## 14. ChattingServer 변경 계획

### 14.1 1차
- 가능하면 서버 수정 없이 현재 `LoginRq` 경로를 그대로 사용한다.
- WinForms 클라이언트가 임시 `userId`를 생성해서 붙으면 지금 구조에서도 빠른 시연이 가능하다.

### 14.2 2차
- `Packet/Login/Login.yaml`에 `ticket login`용 메시지를 추가한다.
- 예시:
  - `TicketLoginRq`
    - `ticket : string`
  - `TicketLoginRp`
    - `success : bool`
    - `accountId : uint64`
    - `resultCode : uint16`
- [`FAuthContent.cpp`](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingServer\Contents\Auth\FAuthContent.cpp)는 아래 두 경로를 모두 처리한다.
  - `PrototypeAuth`
  - `ExternalTicketAuth`

### 14.3 전환 정책
- 초기에는 config로 `AllowInsecurePrototypeLogin=true`를 두고 병행 운영한다.
- WinForms와 LoginServer 연동이 안정화되면 외부 인증 환경에서는 `AllowInsecurePrototypeLogin=false`로 내린다.

## 15. WinForms 구현 순서
1. `WinForms` 프로젝트 생성
2. 현재 `ChattingServer` 접속 가능한 TCP/packet 최소 계층 구현
3. 로그인 화면 구현
4. 회원가입 화면 구현
5. 프로토타입용 `temporaryUserId` 생성 규칙 구현
6. `RoomList` 로딩 및 room 선택 입장 구현
7. 채팅 송수신 UI 구현
8. `Broadcast` 수신 로그 표시
9. 최소 예외 처리와 연결 끊김 복구 메시지 추가

## 16. Node.js LoginServer 구현 순서
1. `LoginServer` 스캐폴드
2. `MySQL`, `Redis` 연결 모듈 추가
3. `register/login` mock API 먼저 추가
4. `accounts` schema 적용
5. 실제 회원가입 저장
6. 실제 로그인 검증
7. `chat ticket` 발급 API 추가
8. `ChattingServer` Redis ticket 소비 경로 추가
9. WinForms를 mock auth에서 external auth로 전환

## 17. 검증 계획

### 17.1 1차 WinForms smoke
- 서버 실행 후 로그인 화면에서 임의 입력으로 접속 가능
- `RoomList`가 로드되는지 확인
- room 선택 후 입장 가능
- 메시지 전송 시 `ChattingRp`를 받는지 확인
- 다른 클라이언트의 `Broadcast`가 보이는지 확인

### 17.2 ExternalAuth smoke
- 회원가입 성공
- 로그인 성공
- Redis ticket 발급 성공
- ticket으로 채팅 서버 입장 성공
- 동일 ticket 재사용 시 실패
- 만료 ticket 사용 시 실패

### 17.3 회귀 체크
- 기존 `ChattingDummyClient` 흐름이 깨지지 않는지 확인
- 기존 `LoginRq/Rp` 경로가 config 의도대로 유지되는지 확인

## 18. 리스크와 대응
- 현재 로그인 packet이 너무 단순하다.
  - 대응: 1차는 그대로 쓰고, 2차에서 additive packet으로 확장한다.
- `WinForms`가 현재 C++ packet framing/cipher를 그대로 맞춰야 한다.
  - 대응: 1차는 필요한 packet subset만 최소 구현한다.
- `회원가입`을 1차부터 진짜로 만들면 UI보다 인증 백엔드가 먼저 커질 수 있다.
  - 대응: 1차는 mock, 2차부터 MySQL 저장으로 전환한다.
- `ChattingServer`에 Redis 의존성이 들어오면 서버 책임이 늘어난다.
  - 대응: 책임은 `ticket consume` 최소 범위로 제한하고, 계정 검증 자체는 Node.js에 둔다.

## 19. 결론
- 지금 방향은 `괜찮다` 수준이 아니라, 현재 구조에서는 꽤 현실적인 분리다.
- 가장 빠른 길은 `WinForms 프로토타입`을 먼저 열고, 로그인/회원가입은 잠시 mock으로 통과시키는 것이다.
- 그 다음 `Node.js LoginServer + MySQL + Redis ticket exchange`를 붙이면, 현재 `ChattingServer`를 크게 흔들지 않고 외부 인증으로 넘어갈 수 있다.
- 즉, 작업 순서는 아래로 고정한다.
  1. `WinForms 프로토타입`
  2. `ticket login packet / ChattingServer auth 확장`
  3. `Node.js LoginServer + MySQL + Redis`
  4. `WinForms external auth 전환`
