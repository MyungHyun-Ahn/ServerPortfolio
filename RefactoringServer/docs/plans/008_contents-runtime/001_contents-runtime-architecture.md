# ContentsRuntime 구조 계획

## 1. 목적
- 레거시 프로젝트의 `콘텐츠 전용 스레드 + 프레임 기반 처리` 패턴은 가져오되, `NetworkLib`와의 결합은 줄인다.
- `NetworkLib`는 네트워크 운반에 집중하고, 콘텐츠 실행 모델은 별도 프로젝트 `ContentsRuntime`가 담당한다.

## 2. 경계 방향
- `ContentsRuntime`는 `NetworkLib` 바깥의 상위 계층이다.
- `NetworkLib`는 `sessionId + opcode + payload`까지만 전달한다.
- `ContentsRuntime`는 아래를 맡는다.
  - `sessionId -> contentId` 라우팅
  - 콘텐츠별 스레드 실행
  - `Enter / Leave / Packet / Frame` 처리
  - 콘텐츠 간 세션 이동

## 3. 책임 분리

### 3.1 `NetworkLib`
- 소켓 I/O
- 세션 수명주기
- 패킷 프레이밍 / 직렬화 / 역직렬화 진입점
- `OnPacketReceived(sessionId, packetView)` 전달

### 3.2 `ContentsRuntime`
- 콘텐츠 등록과 시작/정지
- `sessionId -> contentId` 매핑
- 콘텐츠별 packet inbox
- 콘텐츠 이동(`MoveSession`)
- 프레임 루프(`OnFrame`)

### 3.3 서버 프로젝트
- 실제 콘텐츠 구현
  - `AuthContent`
  - `EchoContent`
  - 이후 `LobbyContent`, `RoomContent`
- 로깅, 옵션, 운영 정책 연결

## 4. 디렉터리 구조
- `ContentsRuntime/Core`
  - `ContentTypes.h`
  - `IContent.h`
  - `FContentThread.h/.cpp`
  - `FContentRuntime.h/.cpp`
- `ContentsRuntime/Bridge`
  - `IContentBridge.h`
- `EchoServer/Contents`
  - `Auth/FAuthContent.*`
  - `Echo/FEchoContent.*`
  - `ContentTypes.h`

## 5. 인터페이스

### 5.1 `IContent`
- 콘텐츠 구현의 최소 인터페이스
- 주요 함수
  - `GetContentId()`
  - `GetTargetFps()`
  - `OnEnter(sessionId, bridge)`
  - `OnLeave(sessionId, bridge)`
  - `OnPacket(sessionId, opcode, payload, bridge)`
  - `OnFrame(delayFrame, bridge)`

### 5.2 `IContentBridge`
- 콘텐츠가 `NetworkLib`를 직접 의존하지 않도록 하는 얇은 브리지
- 주요 함수
  - `SendRaw(sessionId, opcode, buffer, length)`
  - `MoveSession(sessionId, targetContentId)`

### 5.3 `FContentThread`
- 콘텐츠 하나를 담당하는 전용 worker thread
- 기본 처리 순서
  1. `Enter`
  2. `Leave`
  3. `Packet`
  4. `Frame`

### 5.4 `FContentRuntime`
- 전체 콘텐츠 스레드와 세션 라우팅의 관리자
- 주요 함수
  - `RegisterContent`
  - `Start`
  - `Stop`
  - `EnterSession`
  - `LeaveSession`
  - `EnqueuePacket`
  - `MoveSession`

## 6. 패킷 처리 흐름
1. `NetworkLib`가 패킷을 수신한다.
2. 서버 application handler가 `ContentsRuntime.EnqueuePacket(...)`를 호출한다.
3. `ContentsRuntime`가 현재 세션 라우트를 보고 대상 콘텐츠 스레드를 찾는다.
4. borrowed view 수명 문제를 피하기 위해 payload는 owned buffer로 넘긴다.
5. 콘텐츠 스레드는 다음 프레임 루프에서 `OnPacket(...)`을 처리한다.

## 7. 콘텐츠 이동 흐름
1. 예를 들어 `AuthContent`가 `LoginRq`를 처리한다.
2. 성공 시 `MoveSession(sessionId, kEchoContentId)`를 호출한다.
3. `ContentsRuntime`가 라우트를 갱신한다.
4. 기존 콘텐츠에 `Leave`
5. 새 콘텐츠에 `Enter`

## 8. 현재 적용 범위
- `AuthContent`
  - `LoginRq`
  - `LoginRp`
  - 성공 시 `EchoContent`로 이동
- `EchoContent`
  - `EchoRq`
  - `RoomSnapshotRq`

## 9. 주의점
- borrowed view(`string_view`, `bytes_view`)는 콘텐츠 스레드 큐로 직접 넘기지 않는다.
- 콘텐츠 전이 규칙은 [003_content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\003_content-transition-rules.md)를 기준으로 맞춘다.

## 10. 후속 작업
- 콘텐츠 런타임 계측 강화
- packet inbox 경량화
- `Lobby -> Room` 같은 실제 전이 모델 추가
- 장시간 soak 검증
