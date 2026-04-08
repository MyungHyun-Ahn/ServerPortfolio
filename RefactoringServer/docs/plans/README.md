# Plans Guide

## 1. 목적
- `docs/plans`는 `RefactoringServer`의 구현 전에 방향, 범위, 단계, 검증 기준을 정리하는 문서 모음이다.
- 코드보다 먼저 합의해야 하는 구조적 결정과 작업 순서를 기록한다.

## 2. 기본 규칙
- 디렉터리와 문서는 `001_`, `002_` 같은 숫자 prefix를 사용한다.
- 새 plan을 추가하면 [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)와 이 `README`를 함께 갱신한다.
- 완료된 plan도 삭제하지 않고, 후속 문서와 연결될 수 있게 유지한다.

## 3. 현재 디렉터리
- 전체 상태판: [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)
- [001_foundation](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation)
- [002_legacy-mhlib](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\002_legacy-mhlib)
- [003_networklib-crypto](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\003_networklib-crypto)
- [004_networklib-session](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\004_networklib-session)
- [005_networklib-packet-view](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\005_networklib-packet-view)
- [006_packet-schema-tooling](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling)
- [007_networklib-performance](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance)
- [008_contents-runtime](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime)
- [009_chatting_server](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server)
- [010_worldserver](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\010_worldserver)
- [011_benchmark_runner](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\011_benchmark_runner)

## 4. ContentsRuntime 문서
- [010_content-worker-pool-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\010_content-worker-pool-architecture.md)
  - worker pool 전환 계획과 mailbox 구조 초안
- [011_work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\011_work-stealing-and-delegate.md)
  - 초기 worker-global queue 기반 시도와 보류 기록
- [012_mailbox-owner-transfer-work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\012_mailbox-owner-transfer-work-stealing-and-delegate.md)
  - 현재 active 구조인 `mailbox owner transfer` 기반 `delegate / work stealing` 구현 문서

## 5. NetworkLib Performance 문서
- [014_rio-registered-buffer-pool.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\014_rio-registered-buffer-pool.md)
  - `per-send register/deregister`를 `64 KiB region + bucket 기반 send segment pool`로 바꾸는 계획
- [015_rio-send-hot-path-overhead-reduction.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\015_rio-send-hot-path-overhead-reduction.md)
  - `Registered Buffer Pool` 이후 단계
  - `SSendRequestContext`, `RIOSend` batching, submit lock 정리 계획
- [016_rio-direct-ring-observability.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\016_rio-direct-ring-observability.md)
  - `RIO Direct` send ring 사용량과 stall 해석 지표 추가 계획
- [017_rio-direct-failure-path-lock-scope.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\017_rio-direct-failure-path-lock-scope.md)
  - `RIO Direct` 실패 경로의 lock 범위 축소 계획
- [018_rio-direct-cache-pingpong-instrumentation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\018_rio-direct-cache-pingpong-instrumentation.md)
  - `RIO Direct`와 `RIO OwnerThread` 차이를 `prepare 비용 / Direct lock 비용 / cross-thread send ring touch`로 분리 계측하는 계획
- [019_networklib-diagnostics-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\019_networklib-diagnostics-runtime-architecture.md)
  - `NetworkLib/Diagnostics`를 공용 monitoring 매니저와 backend별 TLS runtime으로 분리하는 구조 설계

## 6. Codegen 규칙
### PacketGenerator
- 실행 진입점:
  - [Generate-Packets.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Packets.ps1)
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Packets.cmd)

### ConfigGenerator
- 실행 진입점:
  - [Generate-Configs.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Configs.ps1)
  - [Generate-Configs.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Configs.cmd)

### 통합 실행
- packet/config 스키마를 한 번에 갱신할 때 사용:
  - [Generate-Codegen.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Codegen.ps1)
  - [Generate-Codegen.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Codegen.cmd)

## 7. Packet Schema Tooling 문서
- [003_packet-generator-broadcast-contract.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling\003_packet-generator-broadcast-contract.md)
  - schema에서 `broadcast`를 직접 표현하고 generated packet/router/handler에 반영하는 계획

## 8. ChattingServer 문서
- [001_chattingserver-large-packet-benchmark.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\001_chattingserver-large-packet-benchmark.md)
  - 큰 패킷과 room broadcast fan-out 검증용 `ChattingServer` 샘플 계획
- [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)
  - `ChattingServer` 1차 packet surface와 `송신자 제외 Broadcast` 흐름 고정 계획
- [003_chattingdummy-load-test-client-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\003_chattingdummy-load-test-client-plan.md)
  - 범용 `ClientNetworkLib` 선행 구현과 `ChattingDummyClient` 구현 순서 계획

## 9. BenchmarkRunner 문서
- [001_powershell-benchmark-runner-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\011_benchmark_runner\001_powershell-benchmark-runner-plan.md)
  - `PowerShell + YAML manifest` 기반 벤치마크 실행기 계획
  - 1차 `ChattingServer + ChattingDummyClient`를 기준으로 시작하고, 이후 `Echo`와 다른 시나리오로 확장 가능한 범용 runner 구조

## 10. WorldServer 문서
- [001_cell-task-graph-world.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\010_worldserver\001_cell-task-graph-world.md)
  - 향후 `WorldServer`의 cell task graph 아키텍처 메모
