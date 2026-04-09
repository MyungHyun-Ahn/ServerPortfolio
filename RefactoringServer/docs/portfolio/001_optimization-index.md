# 001 Optimization Index

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: Optimization attempts overview

포트폴리오 관점에서 어떤 병목을 줄이려 했고, 실제로 어떤 수치까지 확인했는지 빠르게 보는 문서다.

## 1. Packet / Serialization 경로
- 시도
  - packet cipher, framing, header 구조 정립
  - `PacketView`, borrowed view, zero-copy recv 경로 정리
  - packet build copy 감소와 serialization hot path 정리
- 대표 수치
  - wire format 경계 3단계 고정: `Transport Header / Content Header / Body`
  - zero-copy recv 검증 적용 범위 4곳: `FPacketReader`, `FPacketWriter`, `FPacketSerialization`, `PacketGenerator`
  - 샘플 적용 1종: `Echo.message -> std::string_view`
- 핵심 결과
  - transport header와 content header 경계를 명확히 분리했다.
  - recv 쪽은 borrowed view 기반 zero-copy 해석 방향으로 정리했다.
  - packet/codegen 구조는 현재 생성기 기반으로 안정화된 상태다.
- 대표 문서
  - [003_networklib-packet-header-v1.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\003_networklib-crypto\003_networklib-packet-header-v1.md)
  - [009_recv-deserialize-zero-copy-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\009_recv-deserialize-zero-copy-review.md)
  - [025_packet-structure-overview-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\025_packet-structure-overview-review.md)

## 2. Dual Backend / Session 구조
- 시도
  - `IOCP`, `RIO`, `Boost.Asio`를 하나의 `IServer` 경계 아래에서 교체 가능하게 분리
  - session/server 책임을 backend별 구현으로 나누고 factory로 선택
- 대표 수치
  - 지원 backend 3종: `IOCP`, `RIO`, `Boost.Asio`
  - 공통 비교 workload 2종: `EchoServer`, `ChattingServer`
  - 현재 대표 benchmark 축 3종: `로컬 Chatting`, `Windows Echo`, `Windows Chatting`
- 핵심 결과
  - public 경계는 유지하면서도 backend 교체 실험이 가능해졌다.
  - 이후 `EchoServer`, `ChattingServer` 비교 실험의 기반이 됐다.
- 대표 문서
  - [012_dual-backend-session-server-split-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\012_dual-backend-session-server-split-review.md)
  - [002_networklib-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\002_networklib-current.md)

## 3. IOCP 개선
- 시도
  - `accept()` 기반 경로를 `AcceptEx + IOCP completion` 기반으로 전환
  - reconnect/stress 상황에서 허용량과 안정성을 검증
- 대표 수치
  - churn 검증 조건: `10 sessions`, `3 minutes`, `reconnectProbabilityPercent=100`, `reconnectDelayMs=250`
  - 연결/종료 합계: `20,100`
  - `AcceptEx completion failed = 0`
  - `AcceptEx repost failed = 0`
  - `SO_UPDATE_ACCEPT_CONTEXT failed = 0`
  - `All session slots are in use = 0`
- 핵심 결과
  - `IOCP` 경로는 현재 비교 기준 backend로 사용할 수 있는 수준까지 정리됐다.
  - 고빈도 reconnect churn에서 accept path 자체는 안정적으로 동작함을 확인했다.
- 대표 문서
  - [013_iocp-acceptex-migration.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\013_iocp-acceptex-migration.md)
  - [017_iocp-acceptex-reconnect-stress-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\017_iocp-acceptex-reconnect-stress-review.md)

## 4. RIO Send Path 최적화
- 시도
  - `RIO Direct`와 `RIO OwnerThread` 두 send dispatch mode 비교
  - registered buffer pool, send ring 관측, failure path lock scope 축소
  - send ring full, dispatch overhead, cache ping-pong 원인 추적
- 대표 수치
  - `Echo / Windows Server / 2h / 16B / 250 sessions`
    - `RIO OwnerThread` vs `IOCP Default`
    - `AvgRecvTPS +101.31%`
    - `AvgCpuPercent -22.91%`
    - `EchoAvgMs 4.872ms -> 2.675ms`
  - 같은 조건에서 `RIO Direct` vs `IOCP Default`
    - `AvgRecvTPS -3.06%`
    - `AvgCpuPercent -0.89%`
    - `EchoAvgMs +0.68%`
  - `Chatting / Windows Server / 1h / 128B / 250 sessions / Random`
    - `RIO OwnerThread` vs `IOCP Default`
    - `chat avg/s +14.21%`
    - `avgCpuPercent -37.45%`
    - `chat RTT avg 20.820ms -> 19.598ms`
- 핵심 결과
  - workload에 따라 `Direct`와 `OwnerThread` 장단점이 다르게 나타난다.
  - `OwnerThread`는 특정 echo/chatting workload에서 강했고, `Direct`는 lock scope와 cross-thread touch 해석이 중요해졌다.
  - 관련 계측과 `TLS collector` 기반 monitoring runtime까지 도입했다.
- 대표 문서
  - [015_rio-send-hot-path-overhead-reduction.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\015_rio-send-hot-path-overhead-reduction.md)
  - [019_rio-session-send-ring-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\019_rio-session-send-ring-review.md)
  - [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
  - [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)
  - [027_echo-rio-cache-pingpong-windowsserver-2h-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\027_echo-rio-cache-pingpong-windowsserver-2h-review.md)
  - [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)

## 5. Diagnostics / Monitoring 구조
- 시도
  - runtime 전역 atomic 누적 대신 TLS shard 기반 수집 패턴 정리
  - `NetworkLib/Diagnostics` 공용 monitoring runtime과 backend별 TLS runtime 분리
- 대표 수치
  - 모니터링 계층 2단 분리: `Common Monitoring Runtime + Backend TLS Runtime`
  - RIO 핵심 진단 축 3개:
    - `send prepare`
    - `cross-thread touch`
    - `direct lock wait/hold`
  - 재현 패키지 1종: [echo-rio-cache-pingpong-2h-2mode.zip](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\packages\echo-rio-cache-pingpong-2h-2mode.zip)
- 핵심 결과
  - 공통 카운터와 고빈도 계측을 분리하는 구조를 확보했다.
  - `RIO` 쪽 계측과 cache invalidation 실험을 위한 기반까지 갖춘 상태다.
- 대표 문서
  - [019_networklib-diagnostics-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\019_networklib-diagnostics-runtime-architecture.md)
  - [024_tls-collector-usage-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\024_tls-collector-usage-review.md)

## 6. ContentsRuntime 최적화
- 시도
  - lock-free hot path, enqueue 비용, observability 추가
  - worker pool, mailbox owner transfer, delegate/work stealing 구조 실험
- 대표 수치
  - mailbox baseline 검증:
    - `250 sessions`
    - `connectsPerSecond=10`
    - `room-change=90%`
    - `holdSeconds=600`
    - `Room 77 OnFrame Sleep 15ms`
    - client 정상 종료, server/client error log 없음
  - owner transfer loaded 검증:
    - `3분 loaded run 1회`
    - `10분 loaded run 1회`
    - 반복적인 delegate/work steal 발생 후에도 `move route commit committed=0` 재현 없음
- 핵심 결과
  - 현재 기준 구조는 mailbox owner transfer 기반 delegate/work stealing이다.
  - 과거 lock-free 실험과 병목 기록은 히스토리로 남기고, 현재 구조는 current 문서와 최신 리뷰로 정착했다.
- 대표 문서
  - [012_mailbox-owner-transfer-work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\012_mailbox-owner-transfer-work-stealing-and-delegate.md)
  - [015_content-worker-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\015_content-worker-pool-review.md)
  - [016_delegate-work-stealing-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\016_delegate-work-stealing-review.md)

## 7. Chatting / Benchmark 도구화
- 시도
  - `ChattingServer`, `ChattingDummyClient`, `ClientNetworkLib` 구현
  - `PowerShell + YAML` 기반 benchmark runner 도입
  - Windows Server 실행 패키지와 self-contained 실행 패키지 준비
- 대표 수치
  - 대표 채팅 benchmark 조건:
    - `4 modes`
    - `1 hour`
    - `250 sessions`
    - `128B`
    - `Random`
    - `EventPollMaxCount = 0`
  - 위 Windows Server run 4개 모두 `Succeeded=True`
  - 모든 run에서
    - `connectSuccess = 250`
    - `loginSuccess = 250`
    - `roomChangeSuccess = 250`
    - `timeout = 0`
    - `permanentFailure = 0`
    - `unexpectedDisconnect = 0`
  - self-contained 재현 패키지 2종:
    - `chatting-random-128b-1h-4mode-poll0.zip`
    - `echo-rio-cache-pingpong-2h-2mode.zip`
- 핵심 결과
  - packet/flow가 고정된 채팅 부하 실험을 반복 실행할 수 있게 됐다.
  - 장시간 benchmark와 Windows Server 결과를 프로젝트 안에 재현 가능 상태로 보관하고 있다.
- 대표 문서
  - [004_chatting-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\004_chatting-current.md)
  - [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)
  - [001_powershell-benchmark-runner-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\011_benchmark_runner\001_powershell-benchmark-runner-plan.md)
