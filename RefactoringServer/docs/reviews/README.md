# Reviews Guide

## 1. 규칙
- `reviews`는 구현 결과, 문제 추적, 위험 분석을 남기는 문서 모음이다.
- 디렉터리와 문서는 모두 숫자 prefix를 사용한다.
- 새 review 문서를 추가하면 인덱스도 같이 갱신한다.

## 2. 현재 디렉터리
- `NetworkLib`: [001_networklib](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib)
- `ContentsRuntime`: [002_contentsruntime](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime)
- `Memory`: [003_memory](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory)
- `Containers`: [004_containers](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers)
- `Diagnostics`: [005_diagnostics](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\005_diagnostics)
- `Logging`: [006_logging](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\006_logging)

## 3. NetworkLib 주요 문서
- [012_dual-backend-session-server-split-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\012_dual-backend-session-server-split-review.md)
- [013_pure-rio-baseline-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\013_pure-rio-baseline-review.md)
- [014_rio-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\014_rio-echo-server-flow-review.md)
- [015_rio-send-dispatch-mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\015_rio-send-dispatch-mode-review.md)
- [016_iocp-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\016_iocp-echo-server-flow-review.md)
- [017_iocp-acceptex-reconnect-stress-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\017_iocp-acceptex-reconnect-stress-review.md)
- [018_networklib-performance-analysis-summary.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\018_networklib-performance-analysis-summary.md)

## 4. ContentsRuntime 주요 문서
- [015_content-worker-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\015_content-worker-pool-review.md)
  - 현재 mailbox 중심 구조 리뷰
- [016_delegate-work-stealing-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\016_delegate-work-stealing-review.md)
  - mailbox owner transfer 기반 `delegate / work stealing` 구현 리뷰
- [017_delegate-migration-backlog-trace-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\017_delegate-migration-backlog-trace-review.md)
  - 실패 원인 추적과 최종 수정 경로 기록

## 5. 읽는 순서
- 현재 구조를 먼저 보려면
  - [015_content-worker-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\015_content-worker-pool-review.md)
  - [016_delegate-work-stealing-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\016_delegate-work-stealing-review.md)
- 실패 원인과 수정 배경을 보려면
  - [017_delegate-migration-backlog-trace-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\017_delegate-migration-backlog-trace-review.md)
