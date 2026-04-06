# Plans Guide

## 1. 목적
- `docs/plans`는 구현 전에 방향, 범위, 단계, 검증 기준을 정리하는 문서 모음이다.
- 코드보다 먼저 고정해야 하는 구조 판단과 작업 순서를 기록한다.

## 2. 기본 규칙
- 디렉터리와 문서는 `001_`, `002_` 같은 숫자 prefix를 사용한다.
- 새 plan을 추가하면 [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)도 같이 갱신한다.
- 완료된 plan은 삭제하지 않고, 완료 또는 보류 상태를 문서 안에 명확히 남긴다.

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
- [009_worldserver](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_worldserver)

## 4. ContentsRuntime 문서
- [010_content-worker-pool-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\010_content-worker-pool-architecture.md)
  - worker pool 전환 계획과 mailbox 구조의 출발점
- [011_work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\011_work-stealing-and-delegate.md)
  - 초기 worker-global queue 기반 시도에 대한 보류 문서
- [012_mailbox-owner-transfer-work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\012_mailbox-owner-transfer-work-stealing-and-delegate.md)
  - 현재 active 구현 기준 문서
  - `mailbox owner transfer` 기반 `delegate / work stealing` 구조와 검증 결과를 정리한다.

## 5. Codegen 규칙
### PacketGenerator
- 수동 실행 진입점
  - [Generate-Packets.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.ps1)
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.cmd)

### ConfigGenerator
- 수동 실행 진입점
  - [Generate-Configs.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Configs.ps1)
  - [Generate-Configs.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Configs.cmd)

### 통합 실행
- packet/config 스키마를 함께 갱신할 때 사용
  - [Generate-Codegen.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Codegen.ps1)
  - [Generate-Codegen.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Codegen.cmd)
