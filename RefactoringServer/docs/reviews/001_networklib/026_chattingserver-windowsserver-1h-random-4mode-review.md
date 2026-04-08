# ChattingServer WindowsServer 1h Random 4-Mode Review

## 1. 목적
- `WindowsServerTest`에 저장된 `ChattingServer` 1시간 benchmark 결과를 기준으로 `IOCP`, `IOCP send buffer 0`, `RIO Direct`, `RIO OwnerThread` 4모드를 비교 정리한다.
- 이번 결과는 `128B` 일반 채팅 크기, `Random` room 분포, `250 sessions`, `1시간` 조건에서의 end-to-end 비교 기록이다.

## 2. 실행 조건
- 결과 루트:
  - [20260408_120518_chatting_random_128b_250_1h_4mode_poll0](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0)
- manifest:
  - [manifest.snapshot.yaml](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\manifest.snapshot.yaml)
- 공통 조건:
  - `MeasureSeconds = 3600`
  - `SessionCount = 250`
  - `ConnectsPerSecond = 20`
  - `SendIntervalMs = 0`
  - `PayloadSizeBytes = 128`
  - `RoomSelectionMode = Random`
  - `EventPollMaxCount = 0`
  - `WorkerThreadCount = 4`
  - `ContentsWorkerThreadCount = 4`
  - `RoomCount = 20`
  - `RoomCapacity = 256`
  - `MaxChatPayloadBytes = 256`
  - `RioSendRingSizeBytes = 65536`

## 3. 전체 요약
- 전체 요약 파일:
  - [sequence-summary.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\sequence-summary.csv)
  - [sequence-summary.json](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\sequence-summary.json)
- 4개 run 모두 `Succeeded=True`
- 모든 run에서
  - `connectSuccess = 250`
  - `loginSuccess = 250`
  - `roomChangeSuccess = 250`
  - `timeout = 0`
  - `permanentFailure = 0`
  - `unexpectedDisconnect = 0`

## 4. 비교 결과
| Mode | Chat avg/s | Broadcast avg/s | avgSendTPS | avgRecvTPS | avgCpuPercent | chat RTT avg ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| IOCP Default | 4062.00 | 49340.96 | 54041.391 | 4110.753 | 53.243 | 20.820 |
| IOCP SendBuf 0 | 3855.86 | 46829.80 | 51297.962 | 3902.638 | 60.441 | 22.081 |
| RIO Direct | 3975.57 | 48272.69 | 52881.856 | 4023.979 | 54.927 | 21.448 |
| RIO OwnerThread | 4639.38 | 56385.47 | 61692.090 | 4690.300 | 33.306 | 19.598 |

### 원본 run 요약
- [iocp_default_128b_250_random_1h/run-summary.json](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\iocp_default_128b_250_random_1h\run-summary.json)
- [iocp_sndbuf0_128b_250_random_1h/run-summary.json](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\iocp_sndbuf0_128b_250_random_1h\run-summary.json)
- [rio_direct_128b_250_random_1h/run-summary.json](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\rio_direct_128b_250_random_1h\run-summary.json)
- [rio_owner_128b_250_random_1h/run-summary.json](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\rio_owner_128b_250_random_1h\run-summary.json)

### RTT 원본
- [iocp_default_128b_250_random_1h/rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\iocp_default_128b_250_random_1h\rtt.csv)
- [iocp_sndbuf0_128b_250_random_1h/rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\iocp_sndbuf0_128b_250_random_1h\rtt.csv)
- [rio_direct_128b_250_random_1h/rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\rio_direct_128b_250_random_1h\rtt.csv)
- [rio_owner_128b_250_random_1h/rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\20260408_120518_chatting_random_128b_250_1h_4mode_poll0\rio_owner_128b_250_random_1h\rtt.csv)

## 5. 해석
- 이번 `Windows Server + Random 분포 + 128B + 250 sessions + 1시간` 조건에서는 `RIO OwnerThread`가 가장 좋았다.
- `RIO OwnerThread`는 `IOCP Default` 대비
  - `chat avg/s` 기준 약 `+14.21%`
  - `avgCpuPercent` 기준 약 `-37.45%`
  - `chat RTT avg` 기준 약 `-5.87%`
- `RIO Direct`는 이번 조건에서 `IOCP Default`보다 약간 뒤처졌다.
  - `chat avg/s` 약 `-2.13%`
  - `avgCpuPercent` 약 `+3.16%`
  - `chat RTT avg` 약 `+3.02%`
- `IOCP SendBuf 0`는 4개 중 가장 불리했다.
  - `chat avg/s` 약 `-5.07%`
  - `avgCpuPercent` 약 `+13.52%`
  - `chat RTT avg` 약 `+6.06%`

## 6. 주의할 점
- 이번 결과는 `Echo`가 아니라 `ChattingServer` end-to-end benchmark다.
- 따라서 transport 자체 성능만이 아니라
  - content routing
  - room fan-out
  - dummy client event 처리
  - same-machine scheduling
  까지 같이 섞여 있다.
- 특히 `run-summary.json` 안의 `ServerStats.sessions`나 `ContentStats.sessions`는 종료 직전 snapshot 값이라, 일부 run에서는 teardown timing 영향으로 steady-state 세션 수와 달라질 수 있다.
- 이번 비교에서는 steady-state 판단을 위해
  - `ClientSummary`
  - `avgSendTPS / avgRecvTPS / avgCpuPercent`
  - `rtt.csv` 마지막 누적 `chatting-response`
  를 주요 기준으로 사용했다.

## 7. 결론
- `WindowsServerTest`의 이번 채팅 benchmark에서는 `RIO OwnerThread`가 가장 좋은 처리량과 가장 낮은 CPU를 보였다.
- `RIO Direct`는 이번 workload에서는 `IOCP Default`를 확실히 이기지 못했다.
- 따라서 현재 기준으로는
  - `ChattingServer` 랜덤 분포 장기 부하: `RIO OwnerThread` 우세
  - `IOCP send buffer 0`: 비추천
  로 정리할 수 있다.

## 8. 후속 과제
- 같은 `Chatting` workload를 로컬 개인 PC와 다시 교차 비교해서 환경 의존성을 분리한다.
- `Random`과 `Hotspot` 결과를 한 summary 문서에서 함께 비교한다.
- `RIO Direct`와 `RIO OwnerThread` 차이를 최근 추가한 계측치와 연결해 원인 분석을 이어간다.
