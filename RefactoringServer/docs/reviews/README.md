# Reviews Guide

## 1. 디렉터리 규칙
- `reviews`는 주제별 디렉터리 기준으로 정리한다.
- 디렉터리와 문서는 모두 `001_`, `002_` 같은 숫자 prefix를 사용한다.
- 새 리뷰 문서를 추가하면 이 인덱스도 함께 갱신한다.

## 2. 현재 리뷰 디렉터리
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

## 4. ContentsRuntime 주요 문서
- 기본 구조 리뷰: [001_contents-runtime-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\001_contents-runtime-review.md)
- 상세 트러블슈팅: [002_contents-runtime-troubleshooting.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\002_contents-runtime-troubleshooting.md)
- 콘텐츠 전이 규칙: [003_content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\003_content-transition-rules.md)
- lock-free hot path 리뷰: [004_contents-runtime-lockfree-hot-path-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\004_contents-runtime-lockfree-hot-path-review.md)
- enqueue 최적화 리뷰: [005_contents-runtime-enqueue-optimization-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\005_contents-runtime-enqueue-optimization-review.md)
- lock-free toggle 정책: [006_contents-runtime-lockfree-toggle-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\006_contents-runtime-lockfree-toggle-policy.md)
- 관측 지표 설명: [007_contents-runtime-observability-metrics.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\007_contents-runtime-observability-metrics.md)
- 디렉터리/브리지 리뷰: [008_contents-runtime-directory-and-bridge-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\008_contents-runtime-directory-and-bridge-review.md)
- lock-free inbox 검증 결과: [009_contents-runtime-lockfree-validation-result.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\009_contents-runtime-lockfree-validation-result.md)
- 로비/룸 멀티 인스턴스 리뷰: [010_lobby-room-multi-instance-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\010_lobby-room-multi-instance-review.md)
- room-flow timeout 요약: [011_room-flow-timeout-summary.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\011_room-flow-timeout-summary.md)
- send-post lost-wakeup 리뷰: [012_send-post-lost-wakeup-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\012_send-post-lost-wakeup-review.md)
- Content Rq / Send 흐름 리뷰: [013_content-rq-dispatch-and-send-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\013_content-rq-dispatch-and-send-review.md)
- contentInstanceId 할당 리뷰: [014_content-instance-id-allocation-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\014_content-instance-id-allocation-review.md)

## 5. 운영 메모
- 상세 로그와 조사 과정은 트러블슈팅 문서에 남긴다.
- 빠르게 현재 상태를 파악하려면 요약 문서를 먼저 보고, 필요하면 세부 리뷰 문서로 들어간다.
- 동일 주제에서 구현 흐름과 성능 분석이 둘 다 필요하면 문서를 분리한다.
