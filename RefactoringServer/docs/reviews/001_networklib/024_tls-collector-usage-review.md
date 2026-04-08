# TLS Collector Usage Review

## 1. 목적
- `Foundation/Diagnostics/Tls/FTlsCollectorRuntime`의 사용 목적과 구조를 정리한다.
- `RTT` 계측과 `RIO cache ping-pong` 계측이 같은 패턴 위에서 어떻게 동작하는지 설명한다.
- 이후 새로운 고빈도 성능 계측을 추가할 때 어떤 절차로 붙여야 하는지 기준을 남긴다.

## 2. 왜 만들었는가
- 고빈도 성능 계측에서 전역 `atomic` 누적은 계측 자체가 추가적인 cache invalidation을 만들 수 있다.
- 특히 이번 `RIO Direct` 성능 분석처럼 `cache ping-pong` 자체를 의심하는 경우에는, 계측 누적 경로도 최대한 thread-local에 머물러야 한다.
- 그래서 hot path 기록은 `TLS shard`에만 수행하고, 로그/스냅샷 시점에만 합산하는 공용 기반이 필요했다.

## 3. 기본 구조

### 3.1 `FTlsCollectorRuntime`
- 위치:
  - [FTlsCollectorRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\Tls\FTlsCollectorRuntime.h)
- 역할:
  - runtime 단위로 등록된 TLS shard 목록을 관리한다.
  - shard 등록/해제와 전체 순회만 담당한다.
- 핵심 구성:
  - `FRegisteredTlsShard`
    - shard base class
    - 생성 시 runtime에 자동 등록
    - 소멸 시 runtime에서 자동 해제
  - `ForEachRegisteredTlsShard(...)`
    - snapshot/log 시점에 모든 shard를 순회할 때 사용

### 3.2 사용 모델
1. runtime 클래스가 `FTlsCollectorRuntime`를 상속한다.
2. 모듈별 shard struct가 `FRegisteredTlsShard`를 상속한다.
3. `thread_local` 저장소에 `runtime* -> shard`를 보관한다.
4. hot path에서는 현재 스레드 shard만 찾아 갱신한다.
5. 외부 노출은 snapshot 시점에 shard를 전부 합산한다.

## 4. RTT 모듈 적용 방식

### 4.1 runtime
- [FRttMetricsRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\Rtt\FRttMetricsRuntime.h)
- `FRttMetricsRuntime`가 `FTlsCollectorRuntime`를 상속한다.

### 4.2 TLS state
- [FRttThreadLocalCollector.cpp](D:\Project\ServerPortfolio\RefactoringServer\Foundation\Diagnostics\Rtt\FRttThreadLocalCollector.cpp)
- `STlsRttState`가 `FRegisteredTlsShard`를 상속한다.
- 각 state는 다음 값을 가진다.
  - `collectorRefCount`
  - `activeBucketStartEpochSeconds`
  - `stageAggregates`

### 4.3 기록 흐름
- worker나 runtime loop에서 `FRttThreadLocalCollector`를 생성한다.
- `BeginRequest`, `RecordSample`, `RecordTimeout`은 현재 스레드의 `STlsRttState`만 갱신한다.
- collector가 마지막으로 파괴될 때 bucket snapshot을 flush하고 TLS map에서 제거한다.

### 4.4 중요 포인트
- 한 스레드에서 같은 runtime에 대한 collector가 여러 개 생길 수 있으므로 `collectorRefCount`를 둔다.
- runtime이 교체되거나 종료된 뒤 남아 있는 stale TLS state를 재사용하지 않기 위해 `GetOwnerRuntime()`를 확인한다.

## 5. RIO 계측 적용 방식

### 5.1 runtime
- [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
- `FRioServer`가 `FTlsCollectorRuntime`를 상속한다.

### 5.2 TLS shard
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- `STlsRioSendInstrumentationShard`가 `FRegisteredTlsShard`를 상속한다.
- 현재 들어간 값:
  - `rioSendPrepareCount`
  - `rioSendPrepareTotalNs`
  - `rioSendPrepareMaxNs`
  - `rioSendRingTouchCount`
  - `rioSendRingCrossThreadTouchCount`
  - `rioDirectSendRingLockCount`
  - `rioDirectSendRingLockWaitTotalNs`
  - `rioDirectSendRingLockWaitMaxNs`
  - `rioDirectSendRingLockHoldTotalNs`
  - `rioDirectSendRingLockHoldMaxNs`

### 5.3 기록 함수
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
  - `RecordSendPrepareSample(...)`
  - `RecordSendRingTouch(...)`
  - `RecordDirectSendRingLockSample(...)`
- 이 함수들은 전역 shared counter를 건드리지 않고, `GetOrCreateTlsRioSendInstrumentationShard(*this)`로 현재 스레드 shard를 얻어 기록한다.

### 5.4 합산 지점
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
  - `SServerStats FRioServer::GetStatsSnapshot() const`
- 여기서 `ForEachRegisteredTlsShard(...)`로 모든 shard를 순회해 `SServerStats`에 합산한다.

## 6. 새 계측치 추가 절차

### 6.1 내부 누적만 필요한 경우
1. shard struct에 필드 추가
2. `RecordXxx()` 함수에서 TLS shard 갱신
3. 필요하면 `GetStatsSnapshot()`에서 합산

### 6.2 외부 로그까지 노출할 경우
1. `BackendTypes.h`의 `SServerStats`에 필드 추가
2. `GetStatsSnapshot()`에서 합산 결과 채우기
3. `EchoServer/Main.cpp`, `ChattingServer/Main.cpp` 같은 headless stats 출력 경로에 키 추가

### 6.3 공용 collector로 분리할 때
- 특정 모듈에 종속되지 않는 성능 계측이라면:
  - `Foundation/Diagnostics/<Module>/...` 아래 runtime + collector를 만든다.
- 현재 모듈 내부 구조와 강하게 묶여 있는 계측이라면:
  - `FRioServer`처럼 runtime 내부 shard로 유지해도 된다.

## 7. 공유 상태가 필요한 경우
- 모든 것을 TLS로만 처리할 수는 없다.
- 예를 들어 `cross-thread touch`는 “직전에 이 session send ring을 누가 만졌는지”를 알아야 하므로 session 공유 상태가 필요하다.
- 현재 구현에서는:
  - TLS shard: 누적 카운터
  - session 공유 상태: `last touch thread id`
로 역할을 분리했다.

관련 위치:
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)

## 8. 이 패턴의 장점
- hot path에서 전역 shared atomic 갱신을 피할 수 있다.
- 로그 시점에만 shard 합산 비용이 들어간다.
- runtime 교체 후 stale TLS shard 재사용을 방지할 수 있다.
- `RTT`, `RIO`, 이후 다른 performance collector에도 같은 틀을 적용할 수 있다.

## 9. 주의점
- `ForEachRegisteredTlsShard(...)`는 snapshot/log 시점에만 호출해야 한다.
- TLS map에 runtime pointer를 key로 쓰므로, stale shard를 재사용하지 않도록 owner runtime 검사가 꼭 필요하다.
- shared relation이 필요한 지표는 TLS만으로 해결하려 하지 말고, 최소 공유 상태만 별도로 유지해야 한다.

## 10. 권장 규칙
- 고빈도 계측의 기본은 `TLS shard first`로 잡는다.
- 전역 `atomic` 누적은 정말 필요할 때만 쓴다.
- `runtime + shard + snapshot merge` 패턴을 유지한다.
- hot path에서 문자열 생성, 동적 할당, 전역 lock은 피한다.
