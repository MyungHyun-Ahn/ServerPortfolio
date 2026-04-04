# Reviews 가이드

## 1. 디렉터리 규칙
- `reviews`는 주제별 디렉터리 기준으로 정리한다.
- 각 디렉터리는 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 각 문서도 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 새 리뷰 문서를 추가하면 이 인덱스도 같이 갱신한다.

## 2. 현재 리뷰 디렉터리
- `NetworkLib` 리뷰: [001_networklib](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib)
- `ContentsRuntime` 리뷰: [002_contentsruntime](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime)
- 메모리 리뷰: [003_memory](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory)
- 락프리 컨테이너 리뷰: [004_containers](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers)
- 진단 모듈 리뷰: [005_diagnostics](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\005_diagnostics)
- 로깅 모듈 리뷰: [006_logging](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\006_logging)

## 3. ContentsRuntime 주요 문서
- 기본 구조 리뷰: [001_contents-runtime-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\001_contents-runtime-review.md)
- 상세 트러블슈팅: [002_contents-runtime-troubleshooting.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\002_contents-runtime-troubleshooting.md)
- 콘텐츠 전이 규칙: [003_content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\003_content-transition-rules.md)
- lock-free hot path 리뷰: [004_contents-runtime-lockfree-hot-path-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\004_contents-runtime-lockfree-hot-path-review.md)
- enqueue 최적화 리뷰: [005_contents-runtime-enqueue-optimization-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\005_contents-runtime-enqueue-optimization-review.md)
- lock-free 토글 정책: [006_contents-runtime-lockfree-toggle-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\006_contents-runtime-lockfree-toggle-policy.md)
- 관측 지표 설명: [007_contents-runtime-observability-metrics.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\007_contents-runtime-observability-metrics.md)
- 디렉터리/브리지 정리 리뷰: [008_contents-runtime-directory-and-bridge-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\008_contents-runtime-directory-and-bridge-review.md)
- lock-free inbox 장시간 검증 결과: [009_contents-runtime-lockfree-validation-result.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\009_contents-runtime-lockfree-validation-result.md)
- 로비/룸 멀티 인스턴스 리뷰: [010_lobby-room-multi-instance-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\010_lobby-room-multi-instance-review.md)
- 현재 room-flow timeout 요약: [011_room-flow-timeout-summary.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\011_room-flow-timeout-summary.md)
- send-post lost-wakeup 리뷰: [012_send-post-lost-wakeup-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\012_send-post-lost-wakeup-review.md)
- Content Rq 처리와 Send 흐름 리뷰: [013_content-rq-dispatch-and-send-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\013_content-rq-dispatch-and-send-review.md)

## 4. 관리 원칙
- 상세 로그와 실험 내역은 트러블슈팅 문서에 남긴다.
- 핵심 원인, 재현 조건, 시도 결과만 빠르게 봐야 하는 이슈는 별도 요약 문서로 분리한다.
- 같은 이슈를 다시 분석할 때는 요약 문서를 먼저 보고, 필요하면 상세 트러블슈팅 문서로 내려간다.
