# Plans 가이드

## 1. 목적
- `docs/plans`는 구현 전에 방향, 범위, 단계, 검증 기준을 정리하는 문서 모음이다.
- 코드보다 먼저 고정해야 하는 구조 판단과 후속 작업 순서를 기록한다.

## 2. 디렉터리 규칙
- 작업 디렉터리는 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 각 디렉터리 안 문서도 같은 숫자 prefix를 사용한다.
- 새 plan 문서를 추가하면 [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)도 같은 커밋에서 갱신한다.

## 3. 현재 구조
- 전체 상태판: [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)
- [001_foundation](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation)
- [002_legacy-mhlib](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\002_legacy-mhlib)
- [003_networklib-crypto](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\003_networklib-crypto)
- [004_networklib-session](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\004_networklib-session)
- [005_networklib-packet-view](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\005_networklib-packet-view)
- [006_packet-schema-tooling](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling)
- [007_networklib-performance](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance)
- [008_contents-runtime](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime)
- [009_worldserver](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_worldserver)

## 4. 작성 원칙
- 구현 설명보다 먼저 방향과 판단 근거를 적는다.
- 완료 보고 문서가 아니라 구현 전 합의 문서라는 관점을 유지한다.
- 검증 조건이 중요하면 문서 안에 명시한다.
- 보류된 계획은 삭제하지 말고 `보류` 또는 `역사 문서`로 상태를 분명히 적는다.

## 5. Codegen 규칙
### PacketGenerator
- 수동 실행 기준이다.
- 실행 진입점
  - [Generate-Packets.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.ps1)
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.cmd)

### ConfigGenerator
- 수동 실행 기준이다.
- 실행 진입점
  - [Generate-Configs.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Configs.ps1)
  - [Generate-Configs.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Configs.cmd)

### 통합 실행
- packet/config 스키마를 함께 갱신할 때는 아래 스크립트를 사용한다.
  - [Generate-Codegen.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Codegen.ps1)
  - [Generate-Codegen.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Codegen.cmd)

## 6. ContentsRuntime 문서 묶음
- [001_contents-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\001_contents-runtime-architecture.md)
- [002_contents-runtime-instrumentation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\002_contents-runtime-instrumentation.md)
- [003_contents-runtime-lockfree-hot-path.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\003_contents-runtime-lockfree-hot-path.md)
- [004_contents-runtime-lockfree-validation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\004_contents-runtime-lockfree-validation.md)
- [005_contents-runtime-directory-refine.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\005_contents-runtime-directory-refine.md)
- [006_content-bridge-expansion.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\006_content-bridge-expansion.md)
- [007_multi-content-instance-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\007_multi-content-instance-architecture.md)
- [008_multi-content-support-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\008_multi-content-support-architecture.md)
- [009_lobby-room-multi-instance-flow.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\009_lobby-room-multi-instance-flow.md)
- [010_content-worker-pool-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\010_content-worker-pool-architecture.md)
- [011_work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\011_work-stealing-and-delegate.md)
  - 현재는 보류 문서다.
  - 기존 `worker-global queue` 기반 delegate / work stealing 시도는 제거됐고, 이후 재검토가 필요하면 `content-owned mailbox` 전제를 기준으로 새로 판단한다.

## 7. Foundation 문서 묶음
- [001_foundation-module-layout.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation\001_foundation-module-layout.md)
- [002_logger-module.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation\002_logger-module.md)
- [003_crash-dump-redesign.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation\003_crash-dump-redesign.md)
- [004_rtt-observability-diagnostics.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation\004_rtt-observability-diagnostics.md)
- [005_content-instance-id-allocation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation\005_content-instance-id-allocation.md)
- [006_content-instance-id-reserve-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation\006_content-instance-id-reserve-policy.md)
- [007_yaml-config-and-config-generator.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation\007_yaml-config-and-config-generator.md)
- [008_config-generator-enum-required-and-template.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation\008_config-generator-enum-required-and-template.md)

## 8. NetworkLib Performance 문서 묶음
- [001_buffer-reuse-and-page-pool.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\001_buffer-reuse-and-page-pool.md)
- [002_instrumentation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\002_instrumentation.md)
- [003_session-context-lightweight.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\003_session-context-lightweight.md)
- [004_performance-benchmark-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\004_performance-benchmark-plan.md)
- [005_build-packet-copy-reduction.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\005_build-packet-copy-reduction.md)
- [006_packet-serialization-optimization.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\006_packet-serialization-optimization.md)
- [007_packet-generator-optimization.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\007_packet-generator-optimization.md)
- [008_3min-benchmark.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\008_3min-benchmark.md)
- [009_recv-deserialize-zero-copy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\009_recv-deserialize-zero-copy.md)
- [010_dual-backend-rio-support.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\010_dual-backend-rio-support.md)
- [011_rio-owner-thread-ab-benchmark.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\011_rio-owner-thread-ab-benchmark.md)
- [012_shared-send-packet-and-broadcast.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\012_shared-send-packet-and-broadcast.md)
- [013_iocp-acceptex-migration.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\013_iocp-acceptex-migration.md)

## 9. WorldServer 문서 묶음
- [001_cell-task-graph-world.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_worldserver\001_cell-task-graph-world.md)
