# ContentsRuntime 리뷰

## 1. 목적
- 레거시 프로젝트의 `콘텐츠 전용 스레드 + 프레임 기반 처리` 패턴을 가져오되, `NetworkLib`와의 결합은 낮추는 방향으로 재구성했다.
- 콘텐츠 스레드 자체를 `NetworkLib` 안에 넣지 않고 별도 프로젝트 `ContentsRuntime`로 분리해 책임 경계를 명확히 했다.
- 1차 목표는 `Login -> AuthContent -> EchoContent` 흐름이 실제로 동작하는지 검증하는 것이었다.

## 2. 이번 작업에서 추가한 구조
### 2.1 프로젝트 분리
- 새 프로젝트: [ContentsRuntime.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\ContentsRuntime.vcxproj)
- `NetworkLib`는 네트워크 I/O와 세션 수명주기만 담당한다.
- `ContentsRuntime`는 콘텐츠 스레드, 콘텐츠 라우팅, 콘텐츠 간 이동을 담당한다.

### 2.2 핵심 구성
- [IContent.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\IContent.h)
  - `OnEnter`, `OnLeave`, `OnPacket`, `OnFrame`를 제공하는 콘텐츠 최소 인터페이스
- [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.h)
  - 콘텐츠 하나를 담당하는 전용 스레드
  - `enter`, `leave`, `packet` 큐를 처리한다
- [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.h)
  - 콘텐츠 등록, 스레드 시작/종료, `sessionId -> contentId` 매핑, 브리지 구현 담당
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)
  - `SendRaw`, `MoveSession`, `DisconnectSession`, `IsSessionAlive`, `GetCurrentContentId` 제공

## 3. 구조가 좋은 점
### 3.1 NetworkLib와 콘텐츠 실행 모델 분리
- `NetworkLib`는 `sessionId + opcode + payload`까지 넘기고 끝난다.
- 콘텐츠 스레드 모델은 `ContentsRuntime`가 따로 소유하므로 네트워크 코어 책임이 불어나지 않는다.

### 3.2 레거시 패턴 계승
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)에서
  - `enterQueue`
  - `leaveQueue`
  - `packetQueue`
  를 순서대로 처리한다.
- 프레임 주기도 `GetTargetFps()` 기준으로 돌기 때문에 레거시의 `ContentsThread + FrameTask` 운영 패턴을 자연스럽게 계승했다.

### 3.3 콘텐츠 경계에서 owned payload 사용
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)의 `EnqueuePacket(...)`는 payload를 `FOwnedPacketEnvelope`로 만들어 콘텐츠 큐에 넣는다.
- 이 단계에서 `string_view`, `bytes_view` 같은 borrowed payload를 직접 넘기지 않기 때문에 콘텐츠 스레드 경계에서 수명 문제가 줄어든다.

## 4. 현재 샘플 적용
- [FAuthContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Auth\FAuthContent.cpp)
  - `LoginRq`를 받고 `LoginRp`를 보낸 뒤 성공 시 `MoveSession`으로 `EchoContent`로 이동
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)
  - `EchoRq`
  - `RoomSnapshotRq`
  처리
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - `OnClientConnected`에서 `AuthContent` 진입
  - `OnPacketReceived`에서 `FContentRuntime.EnqueuePacket(...)`만 호출
  - `OnClientDisconnected`에서 `LeaveSession(...)`

## 5. 검증 상태
- `RefactoringServer.sln` x64 Debug 빌드 성공
- `EchoServer` + `EchoClient` 최소 스모크 성공
  - `Login -> Chat snapshot -> Echo`
  - `echo validation succeeded.` 확인
- 서버 로그에서
  - `auth content enter`
  - `login succeeded`
  - `auth content leave`
  - `echo content enter`
  흐름 확인

## 6. 현재 리스크
### 6.1 지속/반복 시나리오
- 단발 경로는 검증했지만, 반복/장시간 시나리오는 별도 검증이 필요했다.
- 이후 lock-free inbox, race injection, 2시간 검증 플랜이 여기서 이어졌다.

### 6.2 콘텐츠 큐 구조
- 초기 구현은 `std::mutex + std::condition_variable + std::deque` 기반이었다.
- 구조 검증에는 충분하지만, 이후 부하에서 경합이 커질 가능성이 있어 hot path 측정과 경량화가 후속 작업으로 이어졌다.

## 7. 결론
- `ContentsRuntime`를 별도 프로젝트로 분리한 판단은 좋다.
- 레거시의 좋은 실행 패턴은 살리고, `NetworkLib`와의 강한 결합은 피하는 구조가 됐다.
- 이후 과제는 안정성 검증과 콘텐츠 전이 규칙 정착, 그리고 inbox 경량화다.
