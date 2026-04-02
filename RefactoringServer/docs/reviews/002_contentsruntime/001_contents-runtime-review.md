# ContentsRuntime 리뷰

## 1. 목적
- 레거시 프로젝트의 `콘텐츠 전용 스레드 + 프레임 기반 처리` 패턴은 가져오되, `NetworkLib`와의 결합은 낮추는 방향으로 재구성했다.
- 콘텐츠 스레드 자체를 `NetworkLib` 안에 넣지 않고 별도 프로젝트 `ContentsRuntime`로 분리해서 책임 경계를 명확히 했다.
- 현재 1차 목표는 `Login -> AuthContent -> EchoContent` 흐름을 실제로 올려보면서 구조가 성립하는지 검증하는 것이다.

## 2. 이번 작업에서 추가된 핵심 구조

### 2.1 프로젝트 분리
- 새 프로젝트: [ContentsRuntime.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\ContentsRuntime.vcxproj)
- `NetworkLib`는 네트워크 I/O와 세션 수명주기까지만 담당한다.
- `ContentsRuntime`는 콘텐츠 스레드, 콘텐츠 라우팅, 콘텐츠 간 이동을 담당한다.

### 2.2 핵심 타입
- [IContent.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\IContent.h)
  - 콘텐츠 구현의 최소 인터페이스다.
  - `OnEnter`, `OnLeave`, `OnPacket`, `OnFrame`를 제공한다.
- [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentThread.h)
  - 콘텐츠 하나당 전용 스레드를 돌린다.
  - `enter`, `leave`, `packet` 큐를 받고 프레임 단위로 처리한다.
- [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentRuntime.h)
  - 콘텐츠 등록, 스레드 시작/종료, `sessionId -> contentId` 매핑, 브리지 구현을 담당한다.
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)
  - 콘텐츠 쪽에서 네트워크 코어에 직접 접근하지 않고 `SendRaw`, `MoveSession` 같은 좁은 API만 사용하도록 제한한다.

## 3. 구조상 좋은 점

### 3.1 NetworkLib와 콘텐츠 실행 모델이 분리됨
- 기존 고민은 "콘텐츠 스레드를 `NetworkLib`가 갖는 게 맞나"였는데, 이번 구조는 그 문제를 잘 피한다.
- `NetworkLib`는 `sessionId + opcode + payload`까지 넘기고 끝난다.
- 콘텐츠 스레드 모델은 `ContentsRuntime`가 따로 소유하므로 네트워크 코어 책임이 불필요하게 커지지 않는다.

### 3.2 레거시의 운영 패턴은 유지
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentThread.cpp)에서
  - `enterQueue`
  - `leaveQueue`
  - `packetQueue`
  를 분리해 처리한다.
- 프레임 주기도 `GetTargetFps()` 기준으로 돌아가므로 레거시의 `ContentsThread + FrameTask` 패턴을 꽤 자연스럽게 계승했다.

### 3.3 borrowed view를 스레드 경계 밖으로 들고 가지 않음
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentRuntime.cpp)의 `EnqueuePacket(...)`는 payload를 `std::vector<char>`로 한 번 복사해서 `FOwnedPacketEnvelope`로 만든 뒤 콘텐츠 큐에 넣는다.
- 이 설계 덕분에 `string_view`, `bytes_view` borrowed packet을 네트워크 recv callback 수명에 묶인 상태로 콘텐츠 스레드로 넘기는 위험을 피했다.
- 즉 콘텐츠 스레드 경계에서는 항상 owned payload로 처리된다는 점이 현재 구조의 가장 중요한 안전장치다.

## 4. 현재 샘플 적용
- [FAuthContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Auth\FAuthContent.cpp)
  - `LoginRq`를 받고 `LoginRp`를 보낸 뒤 성공 시 `MoveSession`으로 `EchoContent`로 이동시킨다.
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)
  - `EchoRq`
  - `RoomSnapshotRq`
  를 처리한다.
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - `OnClientConnected`에서 `AuthContent`로 진입
  - `OnPacketReceived`에서는 `FContentRuntime.EnqueuePacket(...)`만 호출
  - `OnClientDisconnected`에서 `LeaveSession(...)`
  - `OnServerStarted/Stopped`에서 `ContentsRuntime` 시작/종료

## 5. 현재 검증 상태
- `RefactoringServer.sln` x64 Debug 전체 빌드 성공
- `EchoServer` + `EchoClient` 최소 스모크 성공
  - `Login -> Chat snapshot -> Echo`
  - 클라이언트 `holdSeconds=0` 기준 `echo validation succeeded.` 확인
- 서버 로그에서도 아래 흐름을 확인했다.
  - `auth content enter`
  - `login succeeded`
  - `auth content leave`
  - `echo content enter`

## 6. 현재 한계와 리스크

### 6.1 반복/지속 루프 검증은 아직 부족
- `holdSeconds=0` 단발 경로는 통과했다.
- 다만 `holdSeconds=1` 같은 반복/유지 시나리오는 아직 다시 확인이 필요하다.
- 따라서 현재 구조는 "기본 콘텐츠 진입/이동/패킷 분배"는 확인됐지만, 지속 루프 안정성까지 검증 완료된 상태는 아니다.

### 6.2 큐가 아직 락 기반
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentThread.cpp)는 `std::mutex + std::condition_variable + std::deque` 기반이다.
- 1차 구조 검증에는 충분하지만, 이후 실제 부하가 커지면 큐 구조를 더 최적화할 여지가 있다.

### 6.3 콘텐츠 스레드 경계에서 zero-copy는 포기한 상태
- recv callback 안에서는 borrowed view를 쓸 수 있지만, 콘텐츠 스레드로 넘길 때는 owned payload로 바꾼다.
- 이것은 안전을 위한 의도적인 선택이다.
- 이후 정말 필요하면 콘텐츠 큐 전용 borrowed guard 또는 packet clone 전략을 더 고민해야 한다.

## 7. 결론
- 이번 구조는 레거시의 좋은 운영 패턴을 가져오면서도 `NetworkLib`와의 결합을 낮추는 방향으로 잘 정리됐다.
- 특히 `ContentsRuntime`를 별도 프로젝트로 분리한 판단이 좋다.
- 현재 기준으로는 "구조 성립"과 "최소 런타임 검증"은 완료됐다.
- 다만 반복/장시간 시나리오, 큐 성능, 콘텐츠 경계 복사 정책은 다음 단계에서 더 다듬어야 한다.

## 8. TODO
- `holdSeconds > 0` 반복/지속 시나리오 재검증
- 콘텐츠 스레드 장시간 soak 테스트 추가
- `ContentRuntime` 통계 계측 추가
- 필요 시 콘텐츠 큐 경로 최적화 검토
