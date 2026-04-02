# ContentsRuntime 구조 계획

## 1. 목적
- 레거시 프로젝트의 `콘텐츠 전용 스레드 + 프레임 기반 처리` 운영 패턴은 가져오되, `NetworkLib`와의 결합은 낮춘다.
- 콘텐츠 스레드 구조는 `NetworkLib` 내부가 아니라 별도 프로젝트 `ContentsRuntime`에 둔다.
- `NetworkLib`는 네트워크 I/O와 세션 수명주기까지 담당하고, `ContentsRuntime`는 콘텐츠 실행 모델과 콘텐츠 간 이동을 담당한다.

## 2. 설계 원칙
- 레거시의 구조는 계승한다.
  - 콘텐츠 전용 스레드
  - 프레임 기반 `OnFrame`
  - `Enter / Leave / Packet` 분리 처리
  - 콘텐츠 간 이동
- 레거시의 강결합은 버린다.
  - 전역 `g_NetServer` 의존 제거
  - 세션이 콘텐츠 객체 포인터를 직접 들고 있지 않음
  - `void*` 기반 상태 전달 제거
- 콘텐츠 스레드 경계에서는 borrowed view를 직접 넘기지 않는다.
  - 네트워크 callback 안의 borrowed packet은 필요하면 owned payload로 바꿔서 큐에 넣는다.

## 3. 책임 분리

### 3.1 NetworkLib
- 소켓
- 세션
- 송수신
- 패킷 프레이밍
- 콘텐츠 헤더 파싱
- `sessionId + opcode + payload` 전달

### 3.2 ContentsRuntime
- 콘텐츠 등록
- 콘텐츠 스레드 시작/종료
- `sessionId -> contentId` 매핑
- 콘텐츠별 packet inbox queue
- `MoveSession` 처리
- 콘텐츠 프레임 루프 실행

### 3.3 서버 프로젝트
- 실제 콘텐츠 구현
  - `AuthContent`
  - `EchoContent`
  - 이후 `LobbyContent`, `RoomContent`
- 로거, 옵션, 운영 정책 연결

## 4. 제안 디렉터리 구조
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

## 5. 핵심 인터페이스

### 5.1 `IContent`
- 콘텐츠 구현의 최소 인터페이스
- 책임
  - `GetContentId()`
  - `GetTargetFps()`
  - `OnEnter(sessionId, bridge)`
  - `OnLeave(sessionId, bridge)`
  - `OnPacket(sessionId, opcode, payload, bridge)`
  - `OnFrame(delayFrame, bridge)`

### 5.2 `IContentBridge`
- 콘텐츠가 네트워크 코어에 직접 의존하지 않게 하는 좁은 브리지
- 최소 기능
  - `SendRaw(sessionId, opcode, buffer, length)`
  - `MoveSession(sessionId, targetContentId)`

### 5.3 `FContentThread`
- 콘텐츠 하나당 전용 스레드 1개
- 내부 큐
  - `enterQueue`
  - `leaveQueue`
  - `packetQueue`
- 프레임 루프에서 처리 순서
  1. enter
  2. leave
  3. packet
  4. frame

### 5.4 `FContentRuntime`
- 전체 콘텐츠와 스레드를 관리
- `sessionId -> contentId` 매핑 유지
- `EnterSession`, `LeaveSession`, `EnqueuePacket`, `MoveSession` 제공

## 6. 패킷 처리 흐름
1. `NetworkLib`가 recv를 완료하고 `opcode + payload`를 얻는다.
2. 서버 application handler는 해당 패킷을 `ContentsRuntime.EnqueuePacket(...)`로 넘긴다.
3. `ContentsRuntime`는 현재 세션이 속한 콘텐츠를 찾는다.
4. borrowed view 수명 문제를 피하기 위해 payload를 owned buffer로 만들어 콘텐츠 큐에 넣는다.
5. 콘텐츠 스레드가 다음 루프에서 `OnPacket(...)`을 처리한다.

## 7. 콘텐츠 이동 흐름
1. 예: `AuthContent`가 `LoginRq`를 처리한다.
2. 로그인 성공 시 `bridge.MoveSession(sessionId, kEchoContentId)` 호출
3. `FContentRuntime`가 `sessionContentMap`을 갱신한다.
4. 기존 콘텐츠 스레드에는 `Leave`
5. 새 콘텐츠 스레드에는 `Enter`

## 8. 1차 적용 범위
- `AuthContent`
  - `LoginRq` 처리
  - `LoginRp` 전송
  - 성공 시 `EchoContent`로 이동
- `EchoContent`
  - `EchoRq` 처리
  - `RoomSnapshotRq` 처리

## 9. 검증 기준
- `Login -> Chat snapshot -> Echo` 단발 경로 통과
- `holdSeconds > 0` 반복 경로 통과
- 세션 이동 후 같은 연결에서 정상적으로 echo 반복 가능
- 콘텐츠 스레드 종료 시 `Leave` 정리 정상 동작

## 10. 후속 작업
- 콘텐츠 런타임 계측 추가
- 콘텐츠 큐 성능 최적화
- `Lobby -> Room` 같은 실제 이동 모델 추가
- 장시간 soak 테스트 추가
