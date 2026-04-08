# 018. RIO Direct Cache Ping-Pong 계측 계획

## 1. 목적
- `EchoServer` 기준으로 `RIO Direct`가 `RIO OwnerThread`보다 느린 원인이 진짜 `sendRingMutex` 대기 경합인지, 아니면 서로 다른 스레드가 같은 session send ring 상태를 번갈아 만지면서 생기는 `cache ping-pong`인지 구분한다.
- 성능 최적화 전에 먼저 `prepare 비용`, `Direct lock 비용`, `cross-thread send ring touch`를 수치로 확보한다.
- 이후 `encode / checksum / framing lock 밖 이동`, `Direct hot path lock scope 축소` 실험의 우선순위를 정할 근거를 만든다.

## 2. 현재 가설

### 2.1 주요 가설
- `EchoServer`에서는 같은 session에 대해 여러 producer thread가 동시에 `SendPacket`을 때려서 강한 `sendRingMutex` 대기 경합이 생긴다기보다,
- `content worker`가 `AppendPacketToSendRing` / `TryPrepareNextSend`를 수행하고,
- `RIO owner worker`가 `CompleteCurrentSend`를 수행하면서
- 같은 session send ring 상태(`read/write/used/inFlight offset`)가 서로 다른 코어 사이를 왕복해 `cache line invalidation`이 발생할 가능성이 더 크다.

### 2.2 보조 가설
- `Direct`는 `sendRingMutex` 안에서 `encode / checksum / framing`까지 수행하므로, 실제 lock hold time도 기대보다 길 수 있다.
- `OwnerThread`는 owner queue를 drain하면서 여러 packet을 연속 append한 뒤 `PostSend`를 호출하므로, submit batching 이점이 있을 수 있다.

## 3. 계측 질문
- packet 준비 비용(`encode + checksum + framing`)이 얼마나 큰가?
- `Direct` 경로에서 `sendRingMutex`의 `wait time`과 `hold time`이 실제로 큰가?
- 같은 session send ring을 서로 다른 thread가 번갈아 만지는 빈도가 높은가?
- `OwnerThread` 대비 `Direct`에서만 `cross-thread touch rate`가 유의미하게 높은가?

## 3.1 계측 구현 원칙
- hot path 누적 카운터는 전역 shared atomic으로 바로 합산하지 않는다.
- `Foundation/Diagnostics/Tls/FTlsCollectorRuntime` 기반 공용 TLS collector 패턴을 사용해 thread-local shard에 기록하고,
- 로그 출력 시점 또는 snapshot 시점에만 shard를 합산한다.
- 단, `cross-thread touch` 판정을 위해 필요한 `session last touch thread id`는 session 공유 상태로 유지한다.

## 4. 1차 계측 지표

### 4.1 Packet prepare
- `rioSendPrepareCount`
  - send ring append 전에 packet prepare를 수행한 횟수
- `rioSendPrepareTotalNs`
  - 누적 prepare 시간
- `rioSendPrepareMaxNs`
  - 최대 prepare 시간

### 4.2 Send ring touch
- `rioSendRingTouchCount`
  - send ring hot path를 touch한 총 횟수
- `rioSendRingCrossThreadTouchCount`
  - 직전 touch thread와 현재 touch thread가 달랐던 횟수

### 4.3 Direct sendRingMutex
- `rioDirectSendRingLockCount`
  - `Direct` 경로에서 send ring mutex를 획득한 횟수
- `rioDirectSendRingLockWaitTotalNs`
  - send ring mutex 누적 대기 시간
- `rioDirectSendRingLockWaitMaxNs`
  - send ring mutex 최대 대기 시간
- `rioDirectSendRingLockHoldTotalNs`
  - send ring mutex 누적 보유 시간
- `rioDirectSendRingLockHoldMaxNs`
  - send ring mutex 최대 보유 시간

## 5. 계측 지점

### 5.1 `AppendPacketToSendRing`
- 대상:
  - `prepare 시간`
  - `send ring touch`
  - `Direct` append lock wait/hold
- 해석:
  - 여기서 hold time이 크면 `encode / checksum / framing`을 lock 밖으로 빼는 실험 우선순위가 올라간다.

### 5.2 `PostSend -> TryPrepareNextSend`
- 대상:
  - `send ring touch`
  - `Direct` prepare lock wait/hold
- 해석:
  - submit 직전에도 cross-thread touch가 반복되는지 확인한다.

### 5.3 `HandleSendCompletion -> CompleteCurrentSend`
- 대상:
  - `send ring touch`
  - `Direct` completion lock wait/hold
- 해석:
  - completion thread가 send ring을 touch하면서 producer thread와 번갈아 cache line을 가져가는지 확인한다.

## 6. 실험 기준

### 6.1 1차 실험
- 대상 서버: `EchoServer`
- 비교 모드:
  - `RIO Direct`
  - `RIO OwnerThread`
- 권장 조건:
  - `PayloadSize=16`
  - `SessionCount=250`
  - `IntervalMs=0`
  - `SendThreadCount=1`
  - `ResponsesPerThread=1`
  - `1h` 이상 장기 run 1회 이상

### 6.2 해석 기준
- `rioSendRingCrossThreadTouchCount / rioSendRingTouchCount`가 `Direct`에서 높고 `OwnerThread`에서 낮으면 `cache ping-pong` 가설이 강해진다.
- `rioDirectSendRingLockWaitAvgNs`가 낮고 `rioDirectSendRingLockHoldAvgNs`도 낮은데 성능 차이가 크면, 순수 lock 경합보다는 cache locality 문제가 더 유력하다.
- `rioSendPrepareAvgNs`가 크고 `rioDirectSendRingLockHoldAvgNs`와 함께 움직이면 `prepare 작업의 lock 내 수행`이 주요 원인일 수 있다.

## 7. 후속 액션 분기

### 7.1 Cache ping-pong 우세
- `Direct`에서 send ring state 접근을 owner-thread 쪽으로 더 모을 수 있는지 검토
- `CompleteCurrentSend + TryPrepareNextSend` 경로의 cross-thread state 왕복 축소

### 7.2 Lock hold 우세
- `encode / checksum / framing`을 lock 밖으로 이동
- `Append + TryPrepareNextSend`를 한 번의 lock 구간으로 합치기
- completion path의 재락 획득 횟수 축소

### 7.3 Prepare 비용 우세
- packet prepare 전용 최적화
- checksum / framing 비용 절감
- payload encode path 분리

## 8. 주의점
- 이번 계측은 `steady_clock` 기반 시간 측정을 hot path에 넣기 때문에 절대 성능 수치에는 오버헤드가 들어간다.
- 따라서 계측 build로 얻은 결과는 `Direct vs OwnerThread 상대 비교`와 `원인 분리`에 쓰고,
- 최적화 적용 후 최종 성능 비교는 가능하면 계측을 끈 build 또는 계측 최소화 build로 다시 확인한다.
