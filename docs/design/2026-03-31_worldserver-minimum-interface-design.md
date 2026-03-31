# 기획 노트

## 1. 배경
- 현재 실행 프로젝트는 `EchoServer`이며, `NetworkLib`를 MMORPG 서버용 실행 프로젝트에 직접 연결하기에는 Echo 전용 책임이 많이 섞여 있다.
- `CBaseContents`는 현재 `g_NetServer`, `AcquireSession()`, `SendPQCS()`, `Disconnect()` 같은 서버 내부 경로에 직접 기대고 있다.
- 따라서 바로 `WorldServer` 프로젝트를 추가하기보다, 먼저 "새 실행 프로젝트가 `NetworkLib`에 무엇을 요구해야 하는지"를 최소 인터페이스 수준에서 정리하는 편이 안전하다.

## 2. 목적
- `WorldServer` 같은 MMORPG 전용 실행 프로젝트를 만들 때 필요한 최소 인터페이스 후보를 정의한다.
- 현재 `NetworkLib`의 직접 의존 지점을 기준으로, 어떤 책임을 라이브러리에 두고 어떤 책임을 실행 프로젝트로 밀어낼지 정한다.
- 코드 변경 전 설계 기준을 만들어 이후 리팩터링 범위를 작게 유지한다.

## 3. 현재 관찰 가능한 직접 의존
### 3-1. 콘텐츠 계층의 서버 의존
- `CBaseContents::MoveJobEnqueue()`는 `AcquireSession()`에 직접 의존한다.
- `CBaseContents::ProcessMoveJob()`는 `AcquireSession()`과 `RegisterContents()`에 직접 의존한다.
- `CBaseContents::ProcessRecvMsg()`는 `AcquireSession()`, `Disconnect()`, `SendPQCS()`, `ReleaseSession()`에 직접 의존한다.

### 3-2. 실행 프로젝트의 서버 의존
- `EchoServer`는 접속 허용 여부, accept 후 처리, leave 처리, 에러 처리를 `CNetServer` 가상 함수로 구현한다.
- 콘텐츠 생성과 타이머 등록도 실행 프로젝트 쪽에서 결정한다.

### 3-3. 아직 분리되지 않은 책임
- 세션과 콘텐츠의 매핑 규칙
- 콘텐츠 이동 시 넘기는 `void* objectPtr`
- 패킷을 도메인 의미로 해석하는 디스패치 규칙
- 서버 시작 시 어떤 콘텐츠를 어떤 주기로 실행할지 정하는 정책

## 4. `WorldServer` 최소 인터페이스 후보
### 4-1. 접속 수명주기 인터페이스
- 목적:
  - 실행 프로젝트가 접속 허용, 접속 직후 초기화, 접속 종료 후 정리 책임을 구현할 수 있게 한다.
- 후보 항목:
  - `OnConnectionRequest(ip, port) -> bool`
  - `OnSessionAccepted(sessionId)`
  - `OnSessionDisconnected(sessionId)`
  - `OnServerError(errorCode, errorMessage)`

### 4-2. 콘텐츠 라우팅 인터페이스
- 목적:
  - 세션이 어느 콘텐츠에 속하는지, 언제 이동하는지를 실행 프로젝트 또는 상위 계층이 결정할 수 있게 한다.
- 후보 항목:
  - `RouteSessionToContents(sessionId, targetContentsId, context)`
  - `RemoveSessionFromContents(sessionId)`
  - `GetInitialContentsForSession(sessionId) -> contentsId`

### 4-3. 패킷 송수신 인터페이스
- 목적:
  - 콘텐츠가 서버 내부 구조 대신 "세션에 패킷을 보낸다/끊는다" 수준으로만 의존하게 만든다.
- 후보 항목:
  - `SendToSession(sessionId, buffer)`
  - `EnqueueToSession(sessionId, buffer)`
  - `DisconnectSession(sessionId)`

### 4-4. 콘텐츠 실행 인터페이스
- 목적:
  - `WorldServer`가 어떤 콘텐츠를 몇 개 띄우고 어떤 주기로 루프를 돌릴지 결정할 수 있게 한다.
- 후보 항목:
  - `RegisterContentsTimer(contents, intervalMs)`
  - `CreateContents(type) -> contents instance`
  - `ShutdownContents()`

### 4-5. 세션 컨텍스트 인터페이스
- 목적:
  - `void* objectPtr` 대신 도메인 컨텍스트를 명시적으로 넘길 수 있는 방향을 만든다.
- 후보 항목:
  - `PlayerSessionContext`
  - `MoveContext`
  - `SessionMetadata`

## 5. 책임 분리 원칙
### 5-1. `NetworkLib`에 남길 책임
- IOCP 기반 소켓 수명주기
- 세션 ID 발급과 세션 release 규칙
- 송수신 버퍼와 PQCS 재기동
- 콘텐츠 스레드 실행 기반

### 5-2. `WorldServer`에 둘 책임
- 월드/로비/인증 같은 도메인 콘텐츠 구성
- 패킷을 도메인 명령으로 해석하는 로직
- 플레이어 컨텍스트 생성과 파괴
- 접속 이후 초기 콘텐츠 배치 정책

## 6. 이번 단계에서 아직 하지 않을 것
- 실제 `WorldServer` 프로젝트 생성
- `CBaseContents`의 서버 직접 의존 제거
- `ProcessRecvMsg()` 구조 변경
- `void* objectPtr`의 즉시 교체

## 7. 다음 안전 작업 제안
1. `WorldServer` 실행 프로젝트 골격 문서 작성
2. `PlayerSessionContext` 초안 정의 문서 작성
3. `CBaseContents`에서 수신 경로를 제외한 인터페이스 축소 후보만 문서화

## 8. 결론
### 확정된 사실
- `WorldServer`를 붙이기 전에 최소 인터페이스 후보를 먼저 정리하는 것이 현재 단계에서 가장 안전하다.
- 현재 `NetworkLib`는 수신 경로를 제외하면 비교적 명확하게 "남길 책임"과 "밀어낼 책임"을 구분할 수 있다.

### 다음 확인 항목
- `WorldServer` 골격 문서를 바로 만들지 결정
- `PlayerSessionContext`를 문서로 먼저 정의할지 결정
- `CBaseContents`의 이동/leave 경로 기준 인터페이스 축소 문서를 추가할지 결정
