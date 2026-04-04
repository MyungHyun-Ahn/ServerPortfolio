# Pure RIO Baseline Review

## 1. 목적
- `NetworkLib`에 순수 `RIO` backend를 실제로 동작하는 baseline으로 추가한 결과를 정리한다.
- 이번 단계는 `IOCP + RIO` 하이브리드가 아니라 `RIO_EVENT_COMPLETION` 기반 순수 `RIO` 구현이다.

## 2. 이번 범위
- [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)

핵심 구현:
- worker별 `RIO_CQ + event + owner thread`
- `AcceptEx + WSA_FLAG_REGISTERED_IO` 기반 accept
- 세션별 `RIO_RQ` 생성
- recv staging buffer 등록 후 `RIOReceive`
- send 시 packet buffer 등록 후 `RIOSend`
- `activeSessionCount` 기반 least-loaded worker 배정

## 3. 코드 구조
### 3-1. server
- [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
  - backend lifecycle
  - worker/CQ 관리
  - accept loop
  - send / disconnect / stats

### 3-2. session
- [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
  - `sessionId`, `slotIndex`, `generation`
  - owner worker index
  - `RIO_RQ`
  - recv staging buffer / recv ring buffer
  - recv pending state
  - send queue 통계

### 3-3. factory / public layer
- [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.cpp)
  - `Backend: Rio`면 `FRioServer` 선택
- [IServer](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
  - 상위 레이어는 backend 종류를 직접 모른다.

## 4. 동작 흐름
1. `FRioServer::Start()`가 Winsock, RIO function table, listen socket, AcceptEx, worker CQ를 초기화한다.
2. accept thread가 `AcceptEx`로 `WSA_FLAG_REGISTERED_IO` client socket을 받는다.
3. `AttachAcceptedSocket()`이 빈 slot을 먼저 찾고, 그 slot 기준으로 `FRioSession`, recv registered buffer, `RIO_RQ`를 생성한다.
4. owner worker는 `RIO_EVENT_COMPLETION`으로 깨워지고, `RIODequeueCompletion()`으로 completions를 소비한다.
5. recv completion은 framer / cipher / app handler 경로로 올라간다.
6. send는 packet을 framing한 뒤 temporary registered buffer로 `RIOSend()`를 건다.

## 5. 중요 설계 판단
### 5-1. CQ 단일 owner
- CQ를 여러 스레드가 공유 dequeue하지 않는다.
- worker마다 CQ를 하나 두고, 해당 worker만 소비한다.
- 1차 구현에서 동기화 복잡도를 낮추는 데 유리하다.

### 5-2. 세션 owner 정책
- 세션은 accept 시 worker 하나에 배정된다.
- 배정 기준은 `activeSessionCount` 기반 least-loaded다.
- 세션 migration은 이번 범위에서 하지 않는다.

### 5-3. lock-free 범위
- 이번 baseline은 “정확성 우선”이다.
- session request queue 접근 등 필요한 곳에는 lock을 둔다.
- 이후 hot path 계측 후 lock-free 후보만 분리한다.

## 6. 잡힌 버그
### 6-1. 증상
- 다중 세션에서 `RIOCreateRequestQueue failed. error=10014`가 반복됐다.

### 6-2. 원인
- 초기 구현은 빈 slot을 확정하기 전에 slot 탐색 루프 안에서 `RIOCreateRequestQueue()`를 호출했다.
- slot 0이 이미 사용 중인 경우, 같은 accepted socket으로 `RIO_RQ` 생성을 여러 번 시도할 수 있었다.

### 6-3. 수정
- 먼저 빈 slot을 찾는다.
- 그 slot 기준으로 session / registered buffer / `RIO_RQ`를 한 번만 생성한다.
- 마지막에 slot attach를 수행한다.

### 6-4. 수정 파일
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)

## 7. 검증 결과
- `Debug x64` 솔루션 빌드 성공
- `RIO 1세션` 스모크 성공
- `RIO 20세션 / 15초` 스모크 성공
- `RIO 100세션 / 3분` 회귀 성공
- 기존 `IOCP 100세션 / 3분` 회귀도 유지

로그:
- [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_smoke_20x15s_acceptfix2\server.log)
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_smoke_20x15s_acceptfix2\client.log)
- [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_regression_100x3m\server.log)
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_regression_100x3m\client.log)

## 8. 현재 결론
- `RIO`는 이제 stub이 아니라 실제 baseline backend다.
- 아직 성능 최적화 단계는 아니지만, 상위 `EchoServer` / `ContentsRuntime` 경로를 태우는 데는 충분한 상태다.
- 다음 작업은 장시간 soak, `IOCP` 비교, buffer 등록 비용 최적화다.
