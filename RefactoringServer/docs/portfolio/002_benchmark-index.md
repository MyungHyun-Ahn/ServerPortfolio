# 002 Benchmark Index

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-09  
Scope: Benchmark and test result map

포트폴리오 관점에서 바로 보여줄 만한 benchmark 결과와 원문 리뷰를 모아둔 인덱스다.

## 1. Quick Takeaways
- `EchoServer / Windows Server / 2h / 16B / 250 sessions`
  - `RIO OwnerThread`가 가장 강했다.
  - `IOCP Default` 대비 `AvgRecvTPS +101.31%`, `AvgCpuPercent -22.91%`, `EchoAvgMs 4.872 -> 2.675`
- `ChattingServer / Windows Server / 1h / 128B / Random / 250 sessions`
  - `RIO OwnerThread`가 가장 좋았다.
  - `IOCP Default` 대비 `chat avg/s +14.21%`, `avgCpuPercent -37.45%`, `chat RTT avg 20.820ms -> 19.598ms`
- `ChattingServer / Local / 10m / 128B / Hotspot 90% / 150 sessions`
  - `RIO Direct`가 가장 좋았다.
  - `IOCP` 대비 `chat avg/s +8.15%`, `broadcast avg/s +7.07%`, `chat RTT avg 959.506ms -> 887.294ms`
- `EchoServer / Windows Server / 2h / cache ping-pong instrumentation`
  - `RIO Direct`의 `cross-thread touch rate = 49.99%`
  - `RIO OwnerThread`의 `cross-thread touch rate = 0%`
  - 이번 결과는 단순 lock contention보다 `cross-thread send ring cache ping-pong` 가설을 더 강하게 지지한다.

## 2. Representative Benchmarks

### EchoServer / Windows Server / 2h / 4 modes
대표 문서:
- [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)

원본 결과:
- [sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt)

| 모드 | AvgRecvTPS | AvgCpuPercent | EchoAvgMs |
| --- | ---: | ---: | ---: |
| `RIO OwnerThread` | `106,417.10` | `42.45%` | `2.675` |
| `IOCP Default` | `52,863.30` | `55.06%` | `4.872` |
| `RIO Direct` | `51,247.68` | `54.57%` | `4.905` |
| `IOCP SendBuf 0` | `43,302.77` | `59.61%` | `5.920` |

요약:
- 이 workload에서는 `RIO OwnerThread`가 throughput, CPU, RTT 모두 가장 좋았다.
- `RIO Direct`는 `IOCP Default`와 거의 비슷한 수준에 머물렀다.

### ChattingServer / Windows Server / 1h / Random / 4 modes
대표 문서:
- [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)

원본 결과:
- [20260408_120518_chatting_random_128b_250_1h_4mode_poll0](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0)

| 모드 | Chat avg/s | Broadcast avg/s | AvgCpuPercent | Chat RTT avg |
| --- | ---: | ---: | ---: | ---: |
| `RIO OwnerThread` | `4639.38` | `56385.47` | `33.306` | `19.598 ms` |
| `IOCP Default` | `4062.00` | `49340.96` | `53.243` | `20.820 ms` |
| `RIO Direct` | `3975.57` | `48272.69` | `54.927` | `21.448 ms` |
| `IOCP SendBuf 0` | `3855.86` | `46829.80` | `60.441` | `22.081 ms` |

요약:
- 이 chatting workload에서는 `RIO OwnerThread`가 가장 좋았다.
- `RIO Direct`는 이번 조건에서 `IOCP Default`를 넘지 못했다.

### ChattingServer / Local / 10m / Hotspot 90%
대표 문서:
- [021_chattingserver-128b-10m-backend-benchmark-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\021_chattingserver-128b-10m-backend-benchmark-review.md)

원본 결과:
- [20260407_181046_rio_vs_iocp_128b_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m)

| 모드 | Chat avg/s | Broadcast avg/s | Chat RTT avg |
| --- | ---: | ---: | ---: |
| `RIO Direct` | `165.53` | `18,360.63` | `887.294 ms` |
| `IOCP` | `153.05` | `17,147.36` | `959.506 ms` |
| `RIO OwnerThread` | `146.90` | `16,452.01` | `999.514 ms` |

요약:
- 이 조건에서는 `RIO Direct`가 가장 좋았다.
- `Echo`와 `Chatting`은 같은 transport 비교라도 해석을 분리해야 한다.

## 3. Cache Ping-Pong Validation
대표 문서:
- [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)
- [027_echo-rio-cache-pingpong-windowsserver-2h-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\027_echo-rio-cache-pingpong-windowsserver-2h-review.md)

원본 결과:
- [20260408_173748_echo_rio_cache_pingpong_2h_2mode](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_173748_echo_rio_cache_pingpong_2h_2mode)

| 모드 | ResponseAvgPerSec | AvgRecvTPS | AvgCpuPercent | EchoAvgMs | CrossThreadRate | LockWaitAvgNs | LockHoldAvgNs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `RIO Direct` | `43,125.41` | `43,612.46` | `65.66%` | `5.781` | `49.99%` | `149.48` | `305.43` |
| `RIO OwnerThread` | `57,491.45` | `58,921.53` | `59.05%` | `4.333` | `0.00%` | `0` | `0` |

핵심 해석:
- 이번 실험에서 중요한 건 `touch 총량`이 아니라 `cross-thread touch rate`다.
- `OwnerThread`는 총 touch 수가 더 많아도 더 빠르다.
- 따라서 주원인은 "많이 만져서 느리다"가 아니라, `Direct`에서 send ring ownership이 스레드 사이를 자주 오가며 cache invalidation이 반복되는 구조라고 보는 편이 더 맞다.
- `Direct`의 평균 `lock wait/hold`가 작기 때문에, 단순 lock contention 설명은 이번 결과와 잘 맞지 않는다.

## 4. Related Tooling
- benchmark runner: [scripts/bench/README.md](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\README.md)
- benchmark current guide: [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)
- optimization summary: [001_optimization-index.md](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\001_optimization-index.md)

## 5. Portfolio Reading Order
1. [001_optimization-index.md](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\001_optimization-index.md)
2. [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
3. [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)
4. [027_echo-rio-cache-pingpong-windowsserver-2h-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\027_echo-rio-cache-pingpong-windowsserver-2h-review.md)
