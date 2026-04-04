# Dual Backend RIO Support Plan

## 1. 목적
- `NetworkLib`가 `IOCP`와 `RIO`를 같은 상위 API에서 병행 지원하도록 확장한다.
- `EchoServer`, `ContentsRuntime`, packet handler는 transport 구현체를 직접 모르고 `IServer`만 사용한다.
- `IOCP + RIO` 하이브리드는 이번 단계에서 다루지 않고, 나중에 별도 backend(`FRioIocpServer`)로 분리한다.

## 2. 현재 결론
- 방향은 `IOCP 교체`가 아니라 `IOCP 유지 + 순수 RIO 추가`다.
- 현재 `RIO`는 `RIO_EVENT_COMPLETION` 기반의 순수 RIO backend로 구현한다.
- `RIO_IOCP_COMPLETION` 기반 하이브리드는 같은 클래스에 섞지 않고 후속 backend로 분리한다.

## 3. 현재 구조
### 3-1. Public Layer
- [IServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
- [IApplicationHandler](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)

### 3-2. Backend Layer
- [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
- [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
- [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.h)

### 3-3. Session Layer
- [ISession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\ISession.h)
- [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
- [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)

## 4. 순수 RIO 1차 구현 범위
- `FRioServer` startup / shutdown 구현
- RIO function table 로드
- worker별 `RIO_CQ + event + owner thread` 생성
- `AcceptEx + WSA_FLAG_REGISTERED_IO` 기반 accept 경로
- 세션별 `RIO_RQ` 생성
- recv staging buffer 등록과 `RIOReceive` post
- send 시 packet buffer 등록 후 `RIOSend`
- `RIO_EVENT_COMPLETION` 기반 CQ notification / dequeue
- `session -> owner worker` 배정 정책
  - 현재 정책은 `activeSessionCount` 기반 least-loaded

## 5. 구현 정책
### 5-1. CQ ownership
- 공유 CQ를 여러 스레드가 같이 소비하지 않는다.
- worker마다 CQ를 하나 두고, 해당 CQ는 owner worker thread만 dequeue한다.

### 5-2. session ownership
- 세션은 accept 시 worker 하나에 배정된다.
- 배정 기준은 `activeSessionCount`가 가장 적은 worker다.
- 세션은 disconnect 전까지 owner worker를 바꾸지 않는다.

### 5-3. 동기화 정책
- 1차 구현은 정확성 우선이다.
- session request queue 접근처럼 필요한 곳에는 lock을 허용한다.
- 이후 병목이 확인된 hot path만 lock-free 또는 near lock-free로 전환한다.

## 6. 구현 중 확인된 핵심 이슈
- 다중 세션 초기 구현에서 `RIOCreateRequestQueue failed. error=10014`가 발생했다.
- 원인은 빈 session slot을 확정하기 전에 같은 accepted socket으로 `RIOCreateRequestQueue`를 반복 시도할 수 있었던 구조였다.
- 수정 후에는
  - 먼저 빈 slot을 찾고
  - 그 slot 기준으로 session / registered buffer / request queue를 한 번만 생성
  - 마지막에 slot에 attach
  순서로 바꿨다.

## 7. 현재 검증 결과
- `Debug x64` 솔루션 빌드 성공
- `IOCP 100세션 / 3분` 회귀 성공
- `RIO 1세션` 스모크 성공
- `RIO 20세션 / 15초` 스모크 성공
- `RIO 100세션 / 3분` 회귀 성공

검증 로그 예시:
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_smoke_20x15s_acceptfix2\client.log)
- [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_smoke_20x15s_acceptfix2\server.log)
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_regression_100x3m\client.log)

## 8. 다음 단계
1. `RIO` 장시간 soak과 성능 측정
2. `IOCP` / `RIO` 비교 벤치마크
3. send/recv buffer 등록 비용 최적화
4. hot path lock-free 후보 구간 계측
5. 후속 backend로 `FRioIocpServer` 설계
