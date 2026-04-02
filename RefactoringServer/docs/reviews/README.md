# Reviews 가이드

## 0. 네이밍 규칙
- `reviews`는 카테고리 디렉터리 기준으로 정리한다.
- 새 디렉터리는 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 새 문서도 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 기존 문서는 한 번에 전부 개명하기보다, 현재 기준 문서부터 우선 정리한다.

## 1. 현재 기준 문서
- 컨테이너 관련 리뷰는 [001_lock-free-containers-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers\001_lock-free-containers-review.md)를 본다.
- 컨테이너 테스트 이력은 [002_testing-history.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers\002_testing-history.md)를 본다.
- 콘텐츠 런타임 리뷰는 [001_contents-runtime-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\001_contents-runtime-review.md)를 본다.
- 콘텐츠 런타임 트러블슈팅은 [002_contents-runtime-troubleshooting.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\002_contents-runtime-troubleshooting.md)를 본다.
- 콘텐츠 런타임 전이 규칙은 [003_content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\003_content-transition-rules.md)를 본다.
- 콘텐츠 런타임 락프리 hot path 리뷰는 [004_contents-runtime-lockfree-hot-path-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\004_contents-runtime-lockfree-hot-path-review.md)를 본다.
- 콘텐츠 런타임 enqueue 최적화 리뷰는 [005_contents-runtime-enqueue-optimization-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\005_contents-runtime-enqueue-optimization-review.md)를 본다.
- 콘텐츠 런타임 lock-free 토글 정책은 [006_contents-runtime-lockfree-toggle-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\006_contents-runtime-lockfree-toggle-policy.md)를 본다.
- 콘텐츠 런타임 관측 지표 설명은 [007_contents-runtime-observability-metrics.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\007_contents-runtime-observability-metrics.md)를 본다.
- 진단 모듈 리뷰는 [001_crash-dump-module-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\005_diagnostics\001_crash-dump-module-review.md)를 본다.
- 로깅 관련 리뷰는 [001_logger-module-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\006_logging\001_logger-module-review.md)를 본다.
- 메모리 풀 관련 리뷰는 [001_memory-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory\001_memory-pool-review.md)를 본다.
- 메모리 풀 테스트 이력은 [002_testing-history.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory\002_testing-history.md)를 본다.
- 네트워크 패킷 암호화 리뷰는 [001_packet-cipher-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\001_packet-cipher-review.md)를 본다.
- 네트워크 패킷 프레이밍 리뷰는 [002_packet-framer-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\002_packet-framer-review.md)를 본다.
- 네트워크 세션 recv buffer 리뷰는 [003_session-recv-buffer-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\003_session-recv-buffer-review.md)를 본다.
- 네트워크 세션 송신 큐 리뷰는 [004_session-send-queue-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\004_session-send-queue-review.md)를 본다.
- 네트워크 패킷 생성기/라우팅 리뷰는 [005_packet-codegen-and-routing-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\005_packet-codegen-and-routing-review.md)를 본다.
- 네트워크 패킷 컨테이너 지원 리뷰는 [006_packet-container-support-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\006_packet-container-support-review.md)를 본다.
- 네트워크 패킷 컨테이너 정책은 [007_packet-container-support-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\007_packet-container-support-policy.md)를 본다.
- 네트워크 성능 최적화 리뷰는 [008_performance-optimization-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\008_performance-optimization-review.md)를 본다.
- 네트워크 수신 역직렬화 zero-copy 리뷰는 [009_recv-deserialize-zero-copy-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\009_recv-deserialize-zero-copy-review.md)를 본다.
- 네트워크 bytes view zero-copy 리뷰는 [010_bytes-view-zero-copy-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\010_bytes-view-zero-copy-review.md)를 본다.
- 네트워크 디렉터리/PCH 구조 리뷰는 [011_networklib-structure-and-pch-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\011_networklib-structure-and-pch-review.md)를 본다.

## 2. 문서 배치 원칙
- 테스트 계획과 실행 이력도 `reviews/` 아래 카테고리 문서로 함께 관리한다.
- 같은 주제는 가능한 한 `통합 리뷰 1개 + 테스트 이력 1개` 조합으로 유지한다.
- 예전 초안이나 중복 문서는 현재 카테고리 문서에 흡수한 뒤 제거한다.

## 3. 관리 원칙
- 새 리뷰를 추가할 때는 먼저 기존 통합 문서에 흡수 가능한지 확인한다.
- 테스트 관련 문서는 먼저 해당 카테고리의 `testing-history` 성격 문서에 합칠 수 있는지 확인한다.
- 별도 문서가 꼭 필요할 때만 새 파일을 만든다.
