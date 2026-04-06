# Reviews Guide

## 1. 규칙
- `reviews`는 구현 결과, 문제 추적, 위험 분석을 남기는 문서 모음이다.
- 디렉터리와 문서 모두 `001_`, `002_` 같은 숫자 prefix를 사용한다.
- 새 review 문서를 추가하면 인덱스도 함께 갱신한다.

## 2. 현재 디렉터리
- `NetworkLib`: [001_networklib](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib)
- `ContentsRuntime`: [002_contentsruntime](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime)
- `Memory`: [003_memory](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory)
- `Containers`: [004_containers](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers)
- `Diagnostics`: [005_diagnostics](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\005_diagnostics)
- `Logging`: [006_logging](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\006_logging)

## 3. NetworkLib 주요 문서
- dual-backend 분리 리뷰: [012_dual-backend-session-server-split-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\012_dual-backend-session-server-split-review.md)
- pure RIO baseline 리뷰: [013_pure-rio-baseline-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\013_pure-rio-baseline-review.md)
- RIO EchoServer 흐름 리뷰: [014_rio-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\014_rio-echo-server-flow-review.md)
- RIO send dispatch mode 리뷰: [015_rio-send-dispatch-mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\015_rio-send-dispatch-mode-review.md)
- IOCP EchoServer 흐름 리뷰: [016_iocp-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\016_iocp-echo-server-flow-review.md)
- IOCP AcceptEx 재접속 stress 리뷰: [017_iocp-acceptex-reconnect-stress-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\017_iocp-acceptex-reconnect-stress-review.md)
- NetworkLib 성능 분석 요약: [018_networklib-performance-analysis-summary.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\018_networklib-performance-analysis-summary.md)

## 4. ContentsRuntime 주요 문서
- 기본 구조 리뷰: [001_contents-runtime-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\001_contents-runtime-review.md)
- 트러블슈팅 기록: [002_contents-runtime-troubleshooting.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\002_contents-runtime-troubleshooting.md)
- content 전이 규칙: [003_content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\003_content-transition-rules.md)
- lock-free hot path 리뷰: [004_contents-runtime-lockfree-hot-path-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\004_contents-runtime-lockfree-hot-path-review.md)
- enqueue 최적화 리뷰: [005_contents-runtime-enqueue-optimization-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\005_contents-runtime-enqueue-optimization-review.md)
- lock-free toggle 정책: [006_contents-runtime-lockfree-toggle-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\006_contents-runtime-lockfree-toggle-policy.md)
- 관측 지표 설명: [007_contents-runtime-observability-metrics.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\007_contents-runtime-observability-metrics.md)
- 디렉터리 / 브리지 리뷰: [008_contents-runtime-directory-and-bridge-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\008_contents-runtime-directory-and-bridge-review.md)
- lock-free inbox 검증 결과: [009_contents-runtime-lockfree-validation-result.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\009_contents-runtime-lockfree-validation-result.md)
- lobby / room multi-instance 리뷰: [010_lobby-room-multi-instance-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\010_lobby-room-multi-instance-review.md)
- room-flow timeout 요약: [011_room-flow-timeout-summary.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\011_room-flow-timeout-summary.md)
- send-post lost-wakeup 리뷰: [012_send-post-lost-wakeup-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\012_send-post-lost-wakeup-review.md)
- content Rq / send 흐름 리뷰: [013_content-rq-dispatch-and-send-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\013_content-rq-dispatch-and-send-review.md)
- contentInstanceId 할당 리뷰: [014_content-instance-id-allocation-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\014_content-instance-id-allocation-review.md)
- content worker pool / mailbox 리뷰: [015_content-worker-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\015_content-worker-pool-review.md)
- delegate / work stealing 역사 리뷰: [016_delegate-work-stealing-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\016_delegate-work-stealing-review.md)
- delegate / backlog 추적 역사 리뷰: [017_delegate-migration-backlog-trace-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\017_delegate-migration-backlog-trace-review.md)

## 5. 운영 메모
- 현재 상태를 빨리 보고 싶으면 먼저 요약 문서를 본다.
- 세부 원인이나 추적 과정을 보고 싶으면 개별 리뷰로 들어간다.
- obsolete 된 실험 경로는 삭제하지 말고 역사 문서로 남겨 맥락을 보존한다.
