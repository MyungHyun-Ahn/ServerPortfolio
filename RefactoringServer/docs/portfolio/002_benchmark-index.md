# 002 Benchmark Index

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: Benchmark and test result map

포트폴리오 관점에서 바로 보여줄 benchmark 결과와 재현 경로를 정리한 문서다.

## 1. Quick Takeaways
- `EchoServer / Windows Server / 2h / 16B / 250 sessions`
  - `RIO OwnerThread`가 가장 강했다.
  - `IOCP Default` 대비 `AvgRecvTPS +101.31%`, `AvgCpuPercent -22.91%`, `EchoAvgMs 4.872ms -> 2.675ms`
- `ChattingServer / Windows Server / 1h / 128B / Random / 250 sessions`
  - `RIO OwnerThread`가 가장 좋았다.
  - `IOCP Default` 대비 `chat avg/s +14.21%`, `avgCpuPercent -37.45%`, `chat RTT avg 20.820ms -> 19.598ms`
- `ChattingServer / Local / 10m / 128B / Hotspot 90% / 150 sessions`
  - `RIO Direct`가 가장 좋았다.
  - `IOCP` 대비 `chat avg/s +8.15%`, `broadcast avg/s +7.07%`, `chat RTT avg 959.506ms -> 887.294ms`
- 결론적으로 `RIO`가 항상 같은 방식으로 이기는 건 아니고, `Echo`와 `Chatting`, `Random`과 `Hotspot`, `Direct`와 `OwnerThread`의 조합에 따라 순위가 달라진다.

## 2. 대표 Benchmark 결과
### EchoServer
조건:
- `Windows Server`
- `2 hours`
- `PayloadSize = 16`
- `250 sessions`
- 4모드 비교: `IOCP Default`, `IOCP SendBuf 0`, `RIO Direct`, `RIO OwnerThread`

대표 문서:
- [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)

원본 결과:
- [sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt)

| Mode | AvgRecvTPS | AvgCpuPercent | EchoAvgMs |
| --- | ---: | ---: | ---: |
| `RIO OwnerThread` | `106,417.10` | `42.45%` | `2.675` |
| `IOCP Default` | `52,863.30` | `55.06%` | `4.872` |
| `RIO Direct` | `51,247.68` | `54.57%` | `4.905` |
| `IOCP SendBuf 0` | `43,302.77` | `59.61%` | `5.920` |

요약:
- 이 조건에서는 `RIO OwnerThread`가 throughput, CPU, RTT 모두 가장 좋았다.
- `RIO Direct`는 `IOCP Default`와 거의 동급이었다.
- `IOCP SendBuf 0`는 가장 불리했다.

### ChattingServer
#### Local 10m
조건:
- `Local`
- `10 minutes`
- `PayloadSizeBytes = 128`
- `SessionCount = 150`
- `RoomSelectionMode = Hotspot`
- `HotspotBiasPercent = 90`

대표 문서:
- [021_chattingserver-128b-10m-backend-benchmark-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\021_chattingserver-128b-10m-backend-benchmark-review.md)

원본 결과:
- [20260407_181046_rio_vs_iocp_128b_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m)

| Mode | Chat avg/s | Broadcast avg/s | Chat RTT avg |
| --- | ---: | ---: | ---: |
| `RIO Direct` | `165.53` | `18,360.63` | `887.294 ms` |
| `IOCP` | `153.05` | `17,147.36` | `959.506 ms` |
| `RIO OwnerThread` | `146.90` | `16,452.01` | `999.514 ms` |

요약:
- 이 조건에서는 `RIO Direct`가 가장 좋았다.
- `Chatting`은 `Echo`와 달리 contents/broadcast가 섞여 있어 `OwnerThread`가 항상 우세하지는 않았다.

#### Windows Server 1h Random
조건:
- `Windows Server`
- `1 hour`
- `PayloadSizeBytes = 128`
- `SessionCount = 250`
- `RoomSelectionMode = Random`
- `EventPollMaxCount = 0`

대표 문서:
- [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)

원본 결과:
- [20260408_120518_chatting_random_128b_250_1h_4mode_poll0](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0)

| Mode | Chat avg/s | Broadcast avg/s | AvgCpuPercent | Chat RTT avg |
| --- | ---: | ---: | ---: | ---: |
| `RIO OwnerThread` | `4639.38` | `56385.47` | `33.306` | `19.598 ms` |
| `IOCP Default` | `4062.00` | `49340.96` | `53.243` | `20.820 ms` |
| `RIO Direct` | `3975.57` | `48272.69` | `54.927` | `21.448 ms` |
| `IOCP SendBuf 0` | `3855.86` | `46829.80` | `60.441` | `22.081 ms` |

요약:
- 이 조건에서는 `RIO OwnerThread`가 가장 좋았다.
- `IOCP Default` 대비 `chat avg/s +14.21%`, `avgCpuPercent -37.45%`, `chat RTT avg -5.87%`
- `RIO Direct`는 이 workload에서 `IOCP Default`를 넘지 못했다.

## 3. Cache Ping-Pong 실험 축
분석 문서:
- [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)

계측 계획:
- [018_rio-direct-cache-pingpong-instrumentation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\018_rio-direct-cache-pingpong-instrumentation.md)

실행 패키지:
- [echo-rio-cache-pingpong-2h-2mode.zip](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\packages\echo-rio-cache-pingpong-2h-2mode.zip)

패키지 목적:
- `RIO Direct`와 `RIO OwnerThread`를 각각 `2시간` 실행
- 아래 지표를 함께 비교
- `cross-thread touch`
- `Direct lock wait`
- `Direct lock hold`

현재 상태:
- 패키지와 계측은 준비 완료
- 이 축은 “왜 `Direct`가 특정 workload에서 밀리는가”를 설명하는 후속 검증 단계다.

## 4. 재현 도구
- 채팅 benchmark runner
  - [scripts/bench/README.md](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\README.md)
- 코드 생성 스크립트
  - [scripts/generate/README.md](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\README.md)
- 현재 benchmark 기준 문서
  - [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)

## 5. 포트폴리오에서 보여주기 좋은 흐름
1. [001_optimization-index.md](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\001_optimization-index.md)
2. [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
3. [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)
4. [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)

## 6. 관리 원칙
- 새 benchmark 결과를 추가하면 원문은 `reviews` 또는 `WindowsServerTest`에 남긴다.
- 포트폴리오에는 대표성이 있는 결과만 이 문서에 추가한다.
- 이미 대체된 결과는 여기서 비중을 줄이고, 원문은 히스토리로 남긴다.
