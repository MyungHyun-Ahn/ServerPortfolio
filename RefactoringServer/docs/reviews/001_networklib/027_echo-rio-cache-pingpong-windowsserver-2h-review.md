# 027 Echo RIO Cache Ping-Pong Windows Server 2h Review

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-09  
Scope: EchoServer / Windows Server / 2시간 / RIO Direct vs OwnerThread

## 1. 목적
- 최근 `RIO Direct` 성능 저하 원인이 단순 `sendRingMutex` 대기 시간인지, 아니면 `cross-thread send ring touch`로 인한 cache ping-pong인지 확인한다.
- 이번 실험에서는 아래 계측을 사용했다.
  - `rioSendPrepare*`
  - `rioSendRingTouch*`
  - `rioDirectSendRingLock*`

관련 배경 문서:
- [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)
- [018_rio-direct-cache-pingpong-instrumentation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\018_rio-direct-cache-pingpong-instrumentation.md)

## 2. 실험 조건
- 환경: `Windows Server`
- 실행 시간: `7200초`
- 서버: `EchoServer`
- 세션 수: `250`
- payload 크기: `16B`
- `WorkerThreadCount = 4`
- `SendThreadCount = 1`
- `ResponsesPerThread = 1`
- 비교 모드:
  - `RIO Direct`
  - `RIO OwnerThread`

원본 결과:
- [20260408_173748_echo_rio_cache_pingpong_2h_2mode](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_173748_echo_rio_cache_pingpong_2h_2mode)
- [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_173748_echo_rio_cache_pingpong_2h_2mode\summary.csv)

## 3. 결과 요약

| 모드 | ResponseAvgPerSec | AvgRecvTPS | AvgCpuPercent | EchoAvgMs | EchoMaxMs | CrossThreadRate | LockWaitAvgNs | LockHoldAvgNs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `RIO Direct` | `43,125.41` | `43,612.46` | `65.66%` | `5.781` | `132.867` | `49.99%` | `149.48` | `305.43` |
| `RIO OwnerThread` | `57,491.45` | `58,921.53` | `59.05%` | `4.333` | `569.796` | `0.00%` | `0` | `0` |

`Direct` 대비 `OwnerThread` 변화:
- `ResponseAvgPerSec`: `+33.31%`
- `AvgRecvTPS`: `+35.10%`
- `AvgCpuPercent`: `-10.07%`
- `EchoAvgMs`: `-25.05%`
- `EchoMaxMs`: `+328.85%`

관련 결과 파일:
- [rio_direct_2h/run-summary.json](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_173748_echo_rio_cache_pingpong_2h_2mode\rio_direct_2h\run-summary.json)
- [rio_owner_thread_2h/run-summary.json](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_173748_echo_rio_cache_pingpong_2h_2mode\rio_owner_thread_2h\run-summary.json)

## 4. 핵심 해석

### 4-1. touch 총량 자체는 핵심 지표가 아니다
- `RIO OwnerThread`의 총 send ring touch 수가 더 많은 것은 자연스럽다.
- 이 모드가 실제로 더 많은 요청을 처리했기 때문이다.
- 따라서 여기서 봐야 할 것은 절대적인 `touch count`가 아니라:
  - `cross-thread touch rate`
  - 처리량이 더 높은데도 ownership churn이 낮은지 여부

실제 수치:
- `RIO Direct`
  - `FinalRioSendRingTouchCount = 1,242,154,515`
  - `FinalRioSendRingCrossThreadTouchCount = 620,953,577`
  - `FinalRioSendRingCrossThreadRatePercent = 49.99`
- `RIO OwnerThread`
  - `FinalRioSendRingTouchCount = 1,656,023,088`
  - `FinalRioSendRingCrossThreadTouchCount = 0`
  - `FinalRioSendRingCrossThreadRatePercent = 0`

즉:
- `OwnerThread`는 총 touch 수가 더 많아도 더 빠르다.
- 따라서 병목은 "많이 만져서 느리다"보다, "누가 만졌고 ownership이 얼마나 자주 스레드 사이를 오가느냐"로 보는 편이 더 맞다.

### 4-2. mutex 대기 시간만으로는 설명이 부족하다
- `RIO Direct`의 lock 계측은 아래와 같다.
  - `LockWaitAvgNs = 149.48ns`
  - `LockHoldAvgNs = 305.43ns`
- 0은 아니지만, 이 값만으로 `33~35%` 수준의 처리량 차이를 주된 원인으로 설명하기엔 약하다.
- 따라서 이번 결과에서는 단순 "강한 mutex 경합"보다 `cross-thread cache invalidation` 쪽 설명이 더 설득력 있다.

### 4-3. 이번 실험은 cache ping-pong 가설을 강하게 지지한다
- `RIO Direct`는 send ring touch의 거의 절반이 `cross-thread`였다.
- `RIO OwnerThread`는 send ring ownership을 사실상 단일 스레드에 묶어두었고 `0%`를 기록했다.
- 동시에:
  - 처리량이 더 높고
  - CPU가 더 낮고
  - 평균 echo latency도 더 낮다

따라서 이번 결과는:
- `RIO Direct`가 이 workload에서 `cross-thread send ring ownership churn` 비용을 크게 치르고 있다
- 주된 설명은 lock 대기보다 `cache ping-pong` 쪽이다
라는 해석을 강하게 뒷받침한다.

## 5. 주의할 점
- `RIO OwnerThread`는 평균 처리량과 평균 RTT는 더 좋지만, `EchoMaxMs`는 더 나쁘다.
- 즉 trade-off는 대략 이렇게 보인다.
  - steady-state 효율과 평균값은 좋아짐
  - 대신 owner queue batching이나 일시적 backlog 때문에 tail spike는 더 나빠질 수 있음

따라서 이 실험은:
- `cache ping-pong` 가설을 뒷받침하는 강한 근거
- 하지만 모든 latency 차원에서 `OwnerThread`가 무조건 더 좋다는 뜻은 아님

## 6. 결론
- 이번 결과는 [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)에서 세운 가설을 가장 강하게 검증한 사례다.
- 핵심 결론은 아래와 같다.
  - `send ring touch 총량` 자체가 핵심 지표는 아니다
  - 핵심은 `cross-thread touch rate`다
  - 이 echo workload에서 `RIO Direct`는 주로 `cross-thread send ring cache ping-pong` 때문에 밀리고 있다
  - 단순 lock wait 시간은 부차적 원인으로 보는 것이 더 자연스럽다

## 7. 다음 액션
- 다음 최적화는 `RIO Direct`의 send ring ownership churn을 줄이는 쪽이 우선이다.
- 우선순위:
  1. submit/completion 주변의 cross-thread ownership 전환 축소
  2. critical section 범위 축소
  3. 그 다음에 `encode/checksum/framing`을 lock 밖으로 빼는 실험 재검토

관련 문서:
- [019_networklib-diagnostics-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\019_networklib-diagnostics-runtime-architecture.md)
- [024_tls-collector-usage-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\024_tls-collector-usage-review.md)
