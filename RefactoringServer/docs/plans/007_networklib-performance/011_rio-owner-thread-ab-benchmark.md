# RIO Owner-Thread A/B Benchmark Plan

## 1. 목적
- 현재 순수 `RIO` baseline과 `완전 owner-thread send 구조`를 같은 조건에서 비교한다.
- 비교 대상은 “`RIO`가 더 빠르냐”가 아니라 “`RIO`에서 send ownership을 owner worker로 모으는 것이 실제로 이득이냐”다.
- 처리량, 지연, CPU, worker 편차를 함께 보고 다음 구조를 결정한다.

## 2. 비교 대상
이번 비교는 backend를 둘로 쪼개지 않고 `Backend: Rio` 안에서 send dispatch policy를 바꿔서 진행한다.

설정 방향:
- `Backend: Rio`
- `RioSendDispatchMode: Direct | OwnerThread`

의도:
- 기존 pure `RIO` 경로를 유지한다.
- 같은 `FRioServer` 안에서 send ownership 한 축만 바꿔 A/B를 수행한다.
- 나중에 결과가 좋으면 기본값만 바꾸면 된다.

### A안. 현재 pure RIO baseline
- backend: `Rio`
- `RioSendDispatchMode: Direct`
- recv completion 소비는 owner worker가 담당한다.
- `Send()`는 호출한 스레드에서 바로 `RIOSend()`를 건다.
- 즉 session ownership은 부분 적용 상태다.

### B안. owner-thread send 구조
- backend: `Rio`
- `RioSendDispatchMode: OwnerThread`
- recv completion 소비는 owner worker가 담당한다.
- `Send()`는 owner worker inbox에 enqueue만 한다.
- 실제 `RIOSend()`는 owner worker만 호출한다.
- 목표는 “세션 hot path를 더 owner-thread에 가깝게” 만드는 것이다.

## 3. 이번 비교에서 바꾸지 않을 것
- packet framer / cipher
- recv staging buffer 정책
- registered buffer 기본 전략
- `ContentsRuntime` 라우팅 구조
- 클라이언트 시나리오
- worker 수
- session ownership 기준

즉 이번 비교는 `send ownership` 한 축만 바꿔서 본다.

## 4. 측정 항목 정의
### 4-1. 처리량
- `overall avg recvTPS`
  - 전체 측정 시간 기준 평균 content packet 처리량
  - `total recv packet count / elapsed seconds`
- `overall avg sendTPS`
  - 전체 측정 시간 기준 평균 response packet 처리량
  - `total send packet count / elapsed seconds`
- `overall avg recvBps`
  - 전체 측정 시간 기준 평균 수신 payload byte 처리량
- `overall avg sendBps`
  - 전체 측정 시간 기준 평균 송신 payload byte 처리량
- `minute avg recvTPS/sendTPS`
  - 분 단위 평균 처리량 시계열
  - 워밍업, 편차, tail 구간 확인용
- `responses total`
  - 보조 지표
  - 전체 성공 응답 수는 남기되, 2시간 본실험의 주 비교값은 아니다

### 4-2. 지연
- `RTT avg`
  - 클라이언트 기준 전체 평균 왕복 시간
- `RTT p50`
  - 중앙값
- `RTT p95`
  - tail latency 1차 기준
- `RTT p99`
  - tail latency 2차 기준
- `RTT max`
  - 최악 구간 확인용

RTT는 stage별로 분리한다.
- `login-response`
- `room-list`
- `room-enter`
- `room-change-list`
- `room-change`
- `echo-response`

### 4-3. CPU / 자원
- `cpuPercent`
  - 서버 프로세스 전체 CPU 사용률
- `workingSetMB`
  - 서버 working set
- `peakWorkingSetMB`
  - 서버 peak working set

### 4-4. worker 분산
- `workerActiveSessions`
  - worker별 active session 수
- `workerCompletionTPS`
  - worker별 completion 처리량

### 4-5. owner-thread 전용 항목
이 항목은 B안에서 특히 중요하다.

- `ownerInboxDepth`
  - owner worker send inbox 현재 depth
- `ownerInboxMaxDepth`
  - 테스트 중 최대 depth
- `ownerInboxEnqueueTPS`
  - 초당 enqueue 요청 수
- `ownerInboxDequeueTPS`
  - 초당 dequeue 처리 수
- `sendSubmitLagAvgMs`
  - send 요청 enqueue 시점부터 실제 `RIOSend()` submit까지 평균 지연
- `sendSubmitLagP95Ms`
  - submit 지연 tail latency
- `sendSubmitLagMaxMs`
  - 최악 submit 지연

### 4-6. 안정성
- timeout count
- reconnect count
- disconnect count
- send failure count
- recv failure count
- application-level failure count

## 5. 로그 위치
### 5-1. 클라이언트
- RTT CSV
- stage별 success / failure count

### 5-2. 서버
- 기존 `EchoStats`
- 기존 `ContentStats`
- RIO 전용 worker / owner inbox 통계

## 6. 벤치마크 시나리오
### 6-1. latency 시나리오
- `1 session`
- `holdSeconds=60`
- 목적: handoff 추가가 단일 세션 latency에 주는 영향 확인

### 6-2. 일반 동시성 시나리오
- `100 sessions`
- `holdSeconds=180`
- 목적: 일반적인 회귀 기준

### 6-3. 중간 부하 시나리오
- `250 sessions`
- `holdSeconds=180`
- 목적: owner inbox 효과와 worker 편차 확인

### 6-4. burst 시나리오
- `intervalMs=0`
- `PacketsPerSend=1`
- 목적: hot path handoff 비용과 lock 감소 효과 확인

## 7. 실행 원칙
- 같은 머신에서 연속 실행한다.
- 같은 config를 사용한다.
- 같은 build configuration을 사용한다.
- 로그 수준은 동일하게 맞춘다.
- A/B 사이에 code path 외 다른 최적화를 섞지 않는다.

config 차이는 오직 이 값만 허용한다.
- `RioSendDispatchMode: Direct`
- `RioSendDispatchMode: OwnerThread`

### 자동화 스크립트
- 순차 실행 스크립트:
  - [Run-RioDispatchComparisonSequence.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-RioDispatchComparisonSequence.ps1)
  - [Run-RioDispatchComparisonSequence.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-RioDispatchComparisonSequence.cmd)
- 기본 순서:
  - `RioDirect -> RioOwnerThread -> Iocp`
- 기본 출력:
  - `Out/dispatch_ab_2h_<timestamp>/`
  - mode별 로그 디렉터리
  - `summary.csv`

실행 예:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Run-RioDispatchComparisonSequence.ps1 `
  -SessionCount 250 `
  -HoldSeconds 7200 `
  -RoomCount 80 `
  -RoomCapacity 4
```

## 8. 판단 기준
### B안 채택 우세
- `RTT p95/p99`가 개선되거나 유지된다.
- `recvTPS/sendTPS`가 유지되거나 상승한다.
- CPU가 같거나 더 낮다.
- owner inbox backlog가 안정적으로 관리된다.

### B안 보류
- 처리량 이득이 거의 없고 latency만 나빠진다.
- owner inbox backlog가 누적된다.
- worker 병목이 뚜렷하게 심해진다.

## 9. 구현 순서
1. 현재 `Rio`를 baseline으로 고정
2. `SServerConfig`와 YAML schema에 `RioSendDispatchMode` 추가
3. `FRioServer::Send()`를 `Direct | OwnerThread` 분기 구조로 확장
4. `owner-thread send` 실험 모드 추가
5. owner inbox 통계 추가
6. 클라이언트 RTT / 서버 stats 항목 맞춤
7. 1세션 / 100세션 / 250세션 A/B 실행
8. 결과 표 정리

## 10. 현재 결론
- 지금은 pure `RIO` baseline이 이미 동작한다.
- 다음 단계는 곧바로 lock-free 최적화가 아니라, `owner-thread send`가 실제 이득인지 먼저 수치로 검증하는 것이다.

## 11. 1차 10분 파일럿 결과
비교 조건:
- `250세션`
- `holdSeconds=600`
- `room-count=80`
- `room-capacity=4`
- `room-change=90%`
- recv 관련 timeout `15000ms`

측정 로그:
- `Rio + Direct`
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\client.log)
  - [rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\rtt.csv)
- `Rio + OwnerThread`
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_owner_fix_250x10m_t15_r80\client.log)
  - [rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_owner_fix_250x10m_t15_r80\rtt.csv)
- `Iocp`
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_fix_250x10m_t15_r80\client.log)
  - [rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_fix_250x10m_t15_r80\rtt.csv)

참고 수치:

| Mode | responses total | echo-response avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: |
| Rio Direct | 619436 | 3.584 ms | 8.107 ms | 9.390 ms |
| Rio OwnerThread | 664640 | 34.702 ms | 41.093 ms | 69.929 ms |
| Iocp | 669984 | 4.228 ms | 9.256 ms | 12.053 ms |

관찰:
- `Rio OwnerThread`는 처리량은 소폭 높았지만 RTT 평균이 크게 악화됐다.
- `Rio Direct`와 `Iocp`는 steady-state RTT가 비슷한 수준이었다.
- 이번 1회 파일럿에서는 `Iocp`가 처리량이 가장 높았다.

현재 판단:
- `OwnerThread`는 아직 기본 모드 후보가 아니라 비교용 실험 모드로 보는 게 맞다.
- 다만 이 구간의 처리량 평가는 아직 `responses total` 참고 수준이다.
- 다음 2시간 본실험부터는 `overall avg recvTPS/sendTPS/Bps`를 주 비교값으로 사용한다.
- 이후 `OwnerThread` 경로의 queue/handoff 비용을 더 줄일지, 아니면 `Direct`를 RIO 기본으로 유지할지 판단한다.

## 12. 2시간 본실험 결과
비교 조건:
- 순서: `RioDirect -> RioOwnerThread -> Iocp`
- `250세션`
- `holdSeconds=7200`
- `room-count=80`
- `room-capacity=4`
- `room-change=90%`
- recv 관련 timeout `15000ms`
- 자동 실행 스크립트:
  - [Run-RioDispatchComparisonSequence.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-RioDispatchComparisonSequence.ps1)

결과:
- 요약 CSV: [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\dispatch_ab_2h_20260405_035034_2h\summary.csv)
- 모드별 로그:
  - [rio_direct](D:\Project\ServerPortfolio\RefactoringServer\Out\dispatch_ab_2h_20260405_035034_2h\rio_direct)
  - [rio_owner](D:\Project\ServerPortfolio\RefactoringServer\Out\dispatch_ab_2h_20260405_035034_2h\rio_owner)
  - [iocp](D:\Project\ServerPortfolio\RefactoringServer\Out\dispatch_ab_2h_20260405_035034_2h\iocp)
- 세 모드 모두 성공
  - `client.err.log` 비어 있음
  - 2시간 hold 종료 후 정상 정리

핵심 수치:

| Mode | responses total | avg recvTPS | avg sendTPS | avg recvBps | avg sendBps | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 1768780 | 714.084 | 714.084 | 10199.501 | 456290.961 | 3.766% | 1.578 ms | 1.446 ms | 4.647 ms |
| Rio OwnerThread | 1767533 | 711.470 | 711.370 | 10168.318 | 454156.445 | 3.729% | 1.470 ms | 1.459 ms | 4.778 ms |
| Iocp | 1767073 | 710.884 | 710.884 | 10164.527 | 453679.689 | 3.700% | 1.464 ms | 1.418 ms | 4.651 ms |

관찰:
- `avg TPS/Bps` 기준으로는 `Rio Direct`가 가장 높았다.
- `echo-response avg`는 `Iocp`와 `Rio OwnerThread`가 근소하게 낮았지만 차이는 작았다.
- `room-change-list avg`는 `Iocp`가 가장 낮았고, `Rio Direct`가 근접했다.
- `room-change avg`는 `Rio Direct`와 `Iocp`가 사실상 비슷했고, `Rio OwnerThread`가 약간 더 느렸다.
- `10분 파일럿`에서 보였던 `OwnerThread 처리량 우세`는 2시간 avg 기준에선 유지되지 않았다.

현재 결론:
- `Rio Direct`를 현재 RIO 기본 정책으로 유지하는 것이 가장 합리적이다.
- `Rio OwnerThread`는 correctness와 장시간 안정성은 확보됐지만, 현재 구현 상태로는 기본 채택 이점이 분명하지 않다.
- `Iocp`는 여전히 매우 경쟁력 있고, 현재 수치상 `Rio Direct`와 근소한 차이 비교 구도다.
