# RIO Direct Ring Observability

## 1. 목적
- `RIO Direct` 경로의 `session send ring` 사용량을 운영 통계에서 직접 관찰할 수 있게 만든다.
- 현재 `queuedSendBuffers` 중심 통계는 `OwnerThread` 큐 길이는 보이지만, `Direct`의 실제 병목인 ring 사용 수위는 잘 보이지 않는다.

## 2. 배경
- `RIO Direct`는 현재 `session-local send ring`에 바로 append하고, 세션당 `in-flight send` 1개만 유지한다.
- 그러나 현재 stats는 [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp) 의 `GetStatsSnapshot()` 기준으로 `GetQueuedSendBufferCount()` 합산만 제공한다.
- 이 값은 [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp) 의 owner queue 중심 값이라 `Direct` ring pressure를 설명하지 못한다.

## 3. 목표
- `Direct` 경로에서 다음 값을 최소한 관찰 가능하게 만든다.
  - `sendRingUsedBytes`
  - `sendRingInFlightBytes`
  - `maxObservedSendRingUsedBytes`
  - 필요 시 `sendRingFreeBytes`
- 세션 단위와 서버 집계 단위 둘 다 제공한다.
- 통계 추가가 hot path를 크게 느리게 만들지 않도록 한다.

## 4. 범위
대상 파일:
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- 필요 시 [BackendTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\BackendTypes.h)

비범위:
- send ring 구조 자체 변경
- `OwnerThread` 큐 경량화
- RTT 계측 포맷 변경

## 5. 설계 방향
### 5-1. 세션 내부 계측
- `FRioSession`에 ring 관련 수위를 읽을 수 있는 getter를 추가한다.
- `TryAppendSendPacket()`, `CompleteCurrentSend()`, `CancelPreparedSend()`에서 현재 수위와 최대 수위를 갱신한다.
- 최대 수위는 monotonic max로 관리한다.

### 5-2. 서버 집계 통계
- `GetStatsSnapshot()`에서 전체 세션을 훑으며 다음 집계를 계산한다.
  - `totalSendRingUsedBytes`
  - `maxObservedSendRingUsedBytes`
  - 필요 시 `totalSendRingInFlightBytes`
- 기존 `queuedSendBufferCount`는 유지하되, `RIO Direct` 해석에는 ring 통계를 우선 참고하도록 문서화한다.

### 5-3. 로그/운영 해석
- `send stall` 직전 상태를 해석할 수 있도록 ring 수위를 경고 로그에도 포함한다.
- 고부하 런에서는 `queuedSendBuffers=0`이어도 `ring used`가 높게 치솟는지 확인할 수 있어야 한다.

## 6. 구현 순서
1. `FRioSession`에 ring 수위 getter와 최대 수위 계측 추가
2. `SServerStats` 또는 동등 통계 구조에 ring 관련 필드 추가
3. `FRioServer::GetStatsSnapshot()` 집계 반영
4. `send stall` 경고 로그에 현재 used/free/in-flight 수위 추가
5. 짧은 스모크 후 `Rio Direct` 5분 런으로 수치가 실제로 찍히는지 확인

## 7. 검증 기준 및 결과
### 7-1. 검증 기준
- 빌드 성공
- 기존 `RIO Direct` 기능/안정성 회귀 없음
- `Direct` 런에서 `sendRingUsedBytes` 계열 수치가 0만 찍히지 않고 실제로 변한다
- `send stall` 발생 시 로그만으로 당시 ring 상태를 해석할 수 있다

### 7-2. 구현 및 검증 결과
- 구현 완료.
- `FRioSession`에 다음 관측성 값을 추가했다.
  - `sendRingUsedBytes`
  - `sendRingInFlightBytes`
  - `maxObservedSendRingUsedBytes`
- `FRioServer::GetStatsSnapshot()`과 headless stats 로그에 다음 서버 집계 값을 추가했다.
  - `totalSendRingUsedBytes`
  - `totalSendRingInFlightBytes`
  - `maxSessionSendRingUsedBytes`
  - `maxObservedSessionSendRingUsedBytes`
- `send stall` 경고 로그에도 세션별 `used/free/in-flight/maxObserved` 수위를 함께 남기도록 반영했다.
- 전체 `Debug x64` 빌드 성공.
- `Rio Direct` 30초 고부하 스모크 성공.
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_114226_ring_obs_smoke\summary.csv)
  - `responses=1006195`
  - `AvgSendTPS=16507.2950819672`
  - `EchoAvgMs=7.21`
- [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_114226_ring_obs_smoke\rio_direct\server.log) 에서 `queuedSendBuffers=0`이어도 `totalSendRingUsedBytes`, `totalSendRingInFlightBytes`, `maxObservedSessionSendRingUsedBytes`가 실제 값으로 변하는 것을 확인했다.

## 8. 기대 효과
- `Direct`의 실제 병목을 보이지 않게 만드는 관측성 공백을 줄인다.
- 이후 `Direct` 미세 최적화나 stall 원인 분석을 데이터 기반으로 진행할 수 있다.
