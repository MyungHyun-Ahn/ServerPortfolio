# Backend Abstraction

## 1. 목적
- `IOCP`, `RIO`, `boost.asio`를 같은 상위 서버 코드에서 비교할 수 있게 한다.
- 상위 애플리케이션은 backend 구현체를 직접 모르고 `IServer`만 사용한다.

## 2. 현재 구조
### 2-1. 서버 인터페이스
- `IServer`
  - `Start(const SServerConfig&, IApplicationHandler&)`
  - `Stop()`
  - `Send(uint64_t sessionId, uint16_t opcode, const char* buffer, int32_t length)`
  - `Disconnect(uint64_t sessionId)`
  - `GetBackendKind()`
  - `GetStatsSnapshot()`

### 2-2. 애플리케이션 인터페이스
- `IApplicationHandler`
  - `OnServerStarted`
  - `OnClientConnected`
  - `OnPacketReceived`
  - `OnClientDisconnected`
  - `OnServerStopped`

### 2-3. 세션 인터페이스
- `ISession`
  - `sessionId`
  - `slotIndex`
  - `generation`
  - closing / refcount
  - queued send 통계

## 3. backend 구현 상태
- `Iocp`
  - [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
  - 현재 기준선 backend
- `Rio`
  - [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
  - [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
  - `RIO_EVENT_COMPLETION` 기반 순수 RIO backend baseline
- `BoostAsio`
  - 아직 stub

backend 선택은 [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.h)가 담당한다.

## 4. RIO 현재 정책
- `IOCP + RIO` 하이브리드는 이번 클래스에 섞지 않는다.
- 순수 `RIO`는 `FRioServer`가 맡는다.
- 하이브리드가 필요하면 나중에 `FRioIocpServer` 같은 별도 backend로 분리한다.

## 5. ownership 정책
- worker마다 `RIO_CQ`와 owner thread를 하나 둔다.
- 세션은 accept 시 worker 하나에 배정된다.
- 배정 기준은 `activeSessionCount` 기반 least-loaded다.
- 세션은 disconnect 전까지 owner worker를 바꾸지 않는다.

## 6. 현재 장점
- `EchoServer`는 backend 타입을 직접 모르고 시작할 수 있다.
- `IOCP`와 `RIO`를 같은 config 모델에서 선택할 수 있다.
- 상위 계층을 흔들지 않고 transport backend를 확장할 수 있다.

## 7. 현재 한계
- `RIO`는 baseline 구현 단계라 buffer 등록 비용 최적화가 아직 없다.
- `RIO_IOCP_COMPLETION` 기반 하이브리드는 아직 없다.
- backend별 성능 비교는 더 많은 soak / benchmark가 필요하다.

## 8. 다음 작업
- `RIO` 장시간 검증
- `IOCP` / `RIO` 비교 벤치마크
- send/recv hot path 최적화
- 필요 시 `FRioIocpServer` 설계
