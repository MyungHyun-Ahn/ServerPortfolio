# 019. NetworkLib Diagnostics Runtime Architecture

## 1. 목적
- `FRioServer`, `FIocpServer` 안에 섞여 있는 공통 모니터링 책임과 backend 전용 성능 계측 책임을 분리한다.
- 공통 서버 카운터는 `NetworkLib/Diagnostics`의 공용 매니저에서 관리하고,
- 고빈도 성능 계측은 `TLS runtime` 기반으로 backend별로 분리해 관리하는 구조를 설계한다.
- 최종적으로는 transport 로직과 monitoring 로직을 느슨하게 분리하면서도, hot path 오버헤드는 낮게 유지한다.

## 2. 현재 문제

### 2.1 서버 클래스에 모니터링 책임이 섞여 있다
- `FRioServer`, `FIocpServer`는 transport 동작뿐 아니라 다음까지 직접 관리하고 있다.
  - active/accepted session count
  - sent/received packet/byte count
  - WSA call count
  - backend별 ring/pool/stall 관련 관측치
- 이 구조에서는 transport 변경과 monitoring 변경이 같은 클래스에서 함께 일어난다.

### 2.2 공통 카운터와 고빈도 계측의 성격이 다르다
- `activeSessionCount`, `sentPacketCount` 같은 값은 공통 전역 카운터로 보는 것이 자연스럽다.
- 반면 `prepare time`, `lock wait/hold`, `cross-thread touch` 같은 값은 고빈도 hot path라 전역 shared atomic으로 바로 누적하면 계측 자체가 오버헤드가 된다.
- 따라서 둘을 같은 저장 방식으로 다루면 안 된다.

### 2.3 backend별 확장 포인트가 없다
- 현재 `RIO` 계측은 `FRioServer` 내부 구현과 강하게 결합되어 있다.
- 이후 `IOCP` 전용 계측, `BoostAsio` 전용 계측, queue stall, batching 효율 같은 추가 분석을 붙이려면 backend별 monitoring runtime 확장점이 필요하다.

## 3. 설계 방향

### 3.1 계층 분리
- `Foundation/Diagnostics`
  - 범용 진단 인프라
  - 예: `FTlsCollectorRuntime`
- `NetworkLib/Diagnostics`
  - 서버 공통 모니터링 매니저
  - backend별 성능 계측 runtime
- `Servers/Core`
  - transport 본연의 동작
  - monitoring runtime에 이벤트만 전달

### 3.2 저장 전략 분리
- 공통 서버 카운터:
  - `atomic` 기반 공용 매니저에서 관리
- 고빈도 backend 성능 계측:
  - `TLS shard + snapshot merge`
- cross-thread relation이 필요한 값:
  - session 공유 상태 최소 유지

## 4. 목표 구조

### 4.1 공용 매니저
- 가칭: `FServerMonitoringRuntime`
- 역할:
  - 서버 공통 카운터 관리
  - backend별 monitoring runtime 보유
  - 최종 `SServerStats` snapshot 조립

예상 책임:
- `OnSessionAccepted()`
- `OnSessionClosed()`
- `OnPacketReceived(bytes)`
- `OnPacketSent(bytes)`
- `OnWsaRecvCall()`
- `OnWsaSendCall()`
- `BuildSnapshot(...)`

### 4.2 backend 전용 runtime
- `FRioSendMetricsRuntime`
  - `prepare`, `lock wait/hold`, `cross-thread touch`
  - `FTlsCollectorRuntime` 기반
- `FIocpMetricsRuntime`
  - 1차는 비워둘 수 있음
  - 이후 send batching, queue depth, WSA call locality 같은 값 확장 가능

### 4.3 서버 클래스
- `FRioServer`
  - `m_monitoring` 보유
  - `m_monitoring.GetRioSendMetrics().Record...` 호출
  - transport 관련 session scan 결과만 snapshot 조립에 넘김
- `FIocpServer`
  - `m_monitoring` 보유
  - 공통 카운터는 모두 `m_monitoring`을 통해 기록

## 5. 제안 디렉터리 구조

```text
NetworkLib/
  Diagnostics/
    ServerMonitoringTypes.h
    FServerMonitoringRuntime.h
    FServerMonitoringRuntime.cpp
    Rio/
      FRioSendMetricsRuntime.h
      FRioSendMetricsRuntime.cpp
    Iocp/
      FIocpMetricsRuntime.h
      FIocpMetricsRuntime.cpp
```

공용 기반은 계속 `Foundation`에 둔다.

```text
Foundation/
  Diagnostics/
    Tls/
      FTlsCollectorRuntime.h
```

## 6. 책임 분리 기준

### 6.1 `FServerMonitoringRuntime`가 가져갈 것
- 공통 atomic 카운터
  - `activeSessionCount`
  - `acceptedSessionCount`
  - `receivedPacketCount`
  - `sentPacketCount`
  - `receivedByteCount`
  - `sentByteCount`
  - `wsaRecvCallCount`
  - `wsaSendCallCount`
- backend별 runtime 접근자
- `SServerStats` 기본 필드 조립

### 6.2 backend runtime이 가져갈 것
- `RIO` 전용 send path 계측
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

### 6.3 서버가 계속 직접 관리할 것
- session scan이 필요한 현재 상태값
  - `queuedSendBufferCount`
  - `maxObservedQueuedSendBufferCount`
  - `totalSendRingUsedBytes`
  - `totalSendRingInFlightBytes`
  - `maxCurrentSendRingUsedBytes`
  - `maxObservedSendRingUsedBytes`
- 이유:
  - 이 값들은 session object를 순회해야 계산되는 관측치라서, 1차에서는 runtime이 직접 session 저장소를 소유하지 않는 편이 단순하다.

## 7. Snapshot 조립 모델

### 7.1 공통 필드
- `FServerMonitoringRuntime`가 바로 채운다.

### 7.2 backend 확장 필드
- `FRioSendMetricsRuntime::AppendSnapshot(...)`
- 또는 `FRioSendMetricsRuntime::BuildSnapshotSection()`

### 7.3 서버 현재 상태 필드
- 서버 클래스가 session scan 결과를 지역 구조체로 모은 뒤 `BuildSnapshot(...)`에 넘긴다.

예시 흐름:
1. 서버가 session scan으로 ring/pool 상태를 수집
2. `m_monitoring.BuildSnapshot(sessionSnapshot)` 호출
3. 공통 atomic + TLS backend runtime + session scan 결과를 합쳐 `SServerStats` 반환

## 8. TLS runtime 사용 원칙

### 8.1 TLS로 가야 하는 것
- hot path에서 매우 자주 기록되는 값
- 순간적인 wait/hold/prepare time
- per-thread locality가 중요한 값

### 8.2 TLS로 가면 안 되는 것
- 전역 의미가 분명한 공통 카운터
- session 수처럼 상태 변화 이벤트 기반 카운터

### 8.3 공유 상태 예외
- `cross-thread touch`는 직전 touch thread를 알아야 하므로 session 공유 상태가 필요하다.
- 따라서 “누적은 TLS, 판정에 필요한 최소 상태만 공유”를 원칙으로 한다.

## 9. 마이그레이션 순서

### 9.1 1차
- `FServerMonitoringRuntime` 스캐폴드 추가
- `FRioServer`, `FIocpServer`의 공통 atomic 카운터를 runtime으로 이동

### 9.2 2차
- 현재 `FRioServer` 내부 `TLS shard` 계측을 `FRioSendMetricsRuntime`로 이동
- `FRioServer`는 runtime 호출만 남긴다

### 9.3 3차
- `GetStatsSnapshot()` 조립 책임을 공용 runtime 중심으로 정리
- headless log 출력은 기존 `SServerStats`를 그대로 사용

### 9.4 4차
- 필요하면 `IOCP` 전용 계측 runtime 추가

## 10. 지금 당장 하지 않을 것
- 모든 monitoring 데이터를 하나의 거대한 매니저에 몰아넣는 것
- session scan 기반 관측치까지 전부 runtime이 소유하게 만드는 것
- backend 공통 인터페이스를 과도하게 일반화하는 것
- 별도 monitoring thread를 지금 즉시 추가하는 것

## 11. 기대 효과
- `FRioServer` / `FIocpServer` 헤더의 책임이 줄어든다.
- 공통 카운터와 backend 특화 계측이 구분된다.
- `TLS runtime`은 재사용하되, 실제 metric 정의는 backend별로 독립 유지가 가능하다.
- 이후 벤치마크/로그/리뷰 문서에서 “공통 지표”와 “backend 내부 지표”를 더 명확히 분리해 해석할 수 있다.

## 12. 최종 판단
- 현재 시점에서는 `FTlsCollectorRuntime` 자체를 더 추상화하기보다,
- `NetworkLib/Diagnostics` 아래에
  - 공용 매니저 `FServerMonitoringRuntime`
  - backend별 `TLS runtime`
구조를 두는 것이 가장 균형이 좋다.
- 즉 다음 단계의 방향은
  - `transport logic`
  - `common monitoring`
  - `backend-specific diagnostics`
를 3층으로 나누는 것이다.
