# NetworkLib 콘텐츠 스레드 구조 이식 계획

## 1. 목적
- 레거시 프로젝트의 `콘텐츠 전용 스레드 + 프레임 기반 처리` 패턴을 현재 `RefactoringServer` 구조에 맞게 이식한다.
- 단, 레거시의 강한 결합 방식은 버리고 `NetworkLib`와 콘텐츠 계층 사이의 경계를 좁은 인터페이스로 유지한다.
- 목표는 `NetworkLib`가 콘텐츠 스레드 기반 서버를 올릴 수 있는 기반을 제공하되, 콘텐츠 코드가 네트워크 구현 세부를 직접 알지 않게 만드는 것이다.

## 2. 레거시에서 가져올 구조
- 콘텐츠 전용 스레드
  - 레거시 기준: [`CContentsThread.h`](D:\Project\ServerPortfolio\includes\NetworkLib\CContentsThread.h)
  - 이벤트 큐, 타이머 큐, idle 대기, 단순 work stealing 아이디어를 가진다.
- 콘텐츠 베이스
  - 레거시 기준: [`CBaseContents.h`](D:\Project\ServerPortfolio\includes\NetworkLib\CBaseContents.h)
  - `MoveJob`, `LeaveJob`, `RecvMsg`를 한 프레임 루프에서 순서대로 처리한다.
- 프레임 태스크
  - 레거시 기준: [`ContentsFrameTask.h`](D:\Project\ServerPortfolio\includes\NetworkLib\ContentsFrameTask.h)
  - 콘텐츠별 목표 FPS에 맞춰 주기적으로 실행된다.

## 3. 그대로 가져오지 않을 것
- 전역 `g_NetServer` 의존
- 세션이 콘텐츠 객체 포인터를 직접 소유하는 구조
- `void*` 기반 상태 전달
- 콘텐츠가 네트워크 세부 구현과 세션 내부 상태를 직접 건드리는 구조
- 타이머, 모니터링, 스레드 구현이 한 클래스에 과도하게 몰린 결합

## 4. 새 구조 원칙
- `NetworkLib`는 전송과 세션 수명주기를 담당한다.
- 콘텐츠 스레드는 `sessionId + packet/opcode + payload view`를 받아 게임 로직만 처리한다.
- 네트워크에서 콘텐츠로 넘어가는 경계는 브리지 인터페이스 하나로 제한한다.
- 콘텐츠는 `send`, `disconnect`, `move session` 같은 제한된 API만 호출할 수 있어야 한다.
- 세션은 콘텐츠 객체를 직접 소유하지 않고, 콘텐츠 라우팅에 필요한 최소 식별자만 가진다.

## 5. 제안 디렉터리 구조
- `NetworkLib/Contents/Core`
  - `IContent.h`
  - `FContentThread.h/.cpp`
  - `FContentRuntime.h/.cpp`
  - `FContentFrameTask.h/.cpp`
  - `ContentTypes.h`
- `NetworkLib/Contents/Bridge`
  - `IContentBridge.h`
  - `FNetworkContentBridge.h/.cpp`
- `Contents/<Category>`
  - 실제 게임/샘플 콘텐츠 구현
  - 예: `Contents/Auth`, `Contents/Chat`, `Contents/Echo`

## 6. 제안 네임스페이스
- `NetworkLib::Contents::Core`
- `NetworkLib::Contents::Bridge`
- 실제 콘텐츠 구현은 `Contents::<Category>` 또는 프로젝트별 별도 네임스페이스

## 7. 핵심 타입 초안

### 7.1 `IContent`
- 콘텐츠 한 개의 기본 인터페이스
- 책임
  - `OnEnter(sessionId, context)`
  - `OnLeave(sessionId)`
  - `OnPacket(sessionId, opcode, packetView)`
  - `OnFrame(delayFrame)`
- 콘텐츠는 내부적으로 자신이 관리하는 플레이어/세션 상태를 가진다.

### 7.2 `FContentThread`
- 전용 콘텐츠 스레드 한 개
- 책임
  - 콘텐츠 이벤트 큐 소비
  - 프레임 태스크 스케줄링
  - idle 대기
  - 필요 시 단순 work stealing

### 7.3 `FContentRuntime`
- 전체 콘텐츠 스레드/콘텐츠 인스턴스 관리
- 책임
  - 콘텐츠 등록
  - 콘텐츠별 목표 FPS 설정
  - `session -> content` 라우팅
  - thread start/stop

### 7.4 `IContentBridge`
- 콘텐츠가 네트워크 코어에 의존하지 않고 필요한 동작만 요청하는 브리지
- 책임
  - `Send(sessionId, packet)`
  - `Disconnect(sessionId)`
  - `MoveSession(sessionId, targetContentId)`
  - 필요 시 `GetSessionState(sessionId)` 최소 조회

## 8. 처리 흐름

### 8.1 수신
1. `FIocpServer`가 패킷을 수신하고 framing/deserialize 직전까지 처리
2. `opcode`와 `packet view`를 결정
3. 현재 세션이 매핑된 콘텐츠 ID를 조회
4. 해당 콘텐츠 스레드 inbox queue에 이벤트 enqueue
5. 콘텐츠 스레드 프레임 루프에서 `OnPacket()` 실행

### 8.2 이동
1. 콘텐츠가 `MoveSession(sessionId, targetContentId)` 요청
2. 런타임이 `MoveJob` enqueue
3. 다음 프레임에
   - 기존 콘텐츠 `OnLeave`
   - 새 콘텐츠 `OnEnter`
   - 세션의 콘텐츠 매핑 갱신

### 8.3 종료
1. 네트워크에서 세션 종료 감지
2. 세션이 속한 콘텐츠로 `LeaveJob` enqueue
3. 콘텐츠 프레임에서 정리

## 9. 세션과의 결합 정책
- `FSession`은 콘텐츠 객체 포인터를 직접 들지 않는다.
- `FSession`에는 아래 정도만 둔다.
  - `contentId`
  - 필요 시 `contentThreadIndex`
  - 콘텐츠 계층에서 사용하는 단순 상태 식별자
- 콘텐츠 내부 플레이어 상태는 콘텐츠가 직접 가진다.

## 10. 레거시 구조와의 대응
- 레거시 `CContentsThread`
  - 새 구조 `FContentThread`
- 레거시 `CBaseContents`
  - 새 구조 `IContent` + 콘텐츠별 상태 클래스
- 레거시 `ContentsFrameTask`
  - 새 구조 `FContentFrameTask`
- 레거시 `MoveJob/LeaveJob/RecvMsg`
  - 새 구조에서도 유지
- 레거시 `g_NetServer`
  - 새 구조 `IContentBridge`

## 11. 1차 구현 범위
- `FContentRuntime`, `FContentThread`, `IContentBridge` 골격 추가
- 콘텐츠 1개 또는 2개만 붙여 검증
  - `Auth`
  - `Echo`
- `Login -> Move to Echo -> Echo packet` 흐름이 실제 콘텐츠 스레드 위에서 동작하도록 연결

## 12. 1차 구현에서 하지 않을 것
- 완전한 월드/채널 서버 구조
- 복잡한 work stealing 최적화
- 콘텐츠 간 브로드캐스트 시스템
- DB/Redis 연동
- 플레이어 객체 풀링 고도화

## 13. 기대 효과
- 네트워크 worker와 게임 로직 분리
- 콘텐츠 프레임 단위 처리 가능
- `Login`, `Chat`, `World` 같은 서버 카테고리로 확장 쉬움
- 레거시의 장점은 살리면서 현재 코드베이스의 결합도는 낮출 수 있음

## 14. 위험 요소
- 세션 종료와 콘텐츠 이동이 동시에 일어날 때 상태 경쟁 가능성
- borrowed view 패킷을 콘텐츠 스레드 queue에 넘길 때 수명 문제
- 프레임 지연이 누적되면 콘텐츠 처리 지연이 커질 수 있음

## 15. 선행 규칙
- borrowed view는 네트워크 callback 범위를 벗어나 콘텐츠 큐로 넘기지 않는다.
- 콘텐츠 스레드로 넘길 패킷은
  - owned packet으로 변환하거나
  - 콘텐츠 전용 inbox에서 유효 수명 보장이 있는 형태로 래핑해야 한다.
- 즉 콘텐츠 스레드 구조 도입 시 `view -> owned 변환 경계`를 반드시 같이 설계한다.

## 16. 다음 단계
1. `Contents/Core`, `Contents/Bridge` 디렉터리 생성
2. `IContent`, `FContentThread`, `FContentRuntime` 골격 추가
3. `Auth`/`Echo`를 콘텐츠 스레드 모델로 이식
4. `Login -> Echo` 검증
