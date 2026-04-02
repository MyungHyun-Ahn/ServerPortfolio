# Reviews Guide

## 1. 현재 기준 문서
- 컨테이너 관련 리뷰는 [lock-free-containers-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\containers\lock-free-containers-review.md)를 본다.
- 컨테이너 테스트 이력은 [testing-history.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\containers\testing-history.md)를 본다.
- 콘텐츠 런타임 리뷰는 [contents-runtime-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\contentsruntime\contents-runtime-review.md)를 본다.
- 콘텐츠 런타임 락프리 hot path 리뷰는 [contents-runtime-lockfree-hot-path-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\contentsruntime\contents-runtime-lockfree-hot-path-review.md)를 본다.
- 콘텐츠 런타임 enqueue 최적화 리뷰는 [contents-runtime-enqueue-optimization-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\contentsruntime\contents-runtime-enqueue-optimization-review.md)를 본다.
- 콘텐츠 런타임 전이 규칙은 [content-transition-rules.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\contentsruntime\content-transition-rules.md)를 본다.
- 콘텐츠 런타임 트러블슈팅은 [contents-runtime-troubleshooting.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\contentsruntime\contents-runtime-troubleshooting.md)를 본다.
- 진단 모듈 리뷰는 [crash-dump-module-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\diagnostics\crash-dump-module-review.md)를 본다.
- 로깅 관련 리뷰는 [logger-module-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\logging\logger-module-review.md)를 본다.
- 메모리 풀 관련 리뷰는 [memory-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\memory\memory-pool-review.md)를 본다.
- 메모리 풀 테스트 이력은 [testing-history.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\memory\testing-history.md)를 본다.
- 네트워크 패킷 암호화 리뷰는 [packet-cipher-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-cipher-review.md)를 본다.
- 네트워크 패킷 프레이밍 리뷰는 [packet-framer-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-framer-review.md)를 본다.
- 네트워크 패킷 생성기/라우팅 리뷰는 [packet-codegen-and-routing-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-codegen-and-routing-review.md)를 본다.
- 네트워크 패킷 컨테이너 지원 리뷰는 [packet-container-support-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-container-support-review.md)를 본다.
- 네트워크 패킷 컨테이너 정책은 [packet-container-support-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-container-support-policy.md)를 본다.
- 네트워크 bytes view zero-copy 리뷰는 [bytes-view-zero-copy-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\bytes-view-zero-copy-review.md)를 본다.
- 네트워크 디렉터리/PCH 구조 리뷰는 [networklib-structure-and-pch-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\networklib-structure-and-pch-review.md)를 본다.
- 네트워크 수신 역직렬화 zero-copy 리뷰는 [recv-deserialize-zero-copy-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\recv-deserialize-zero-copy-review.md)를 본다.
- 네트워크 성능 최적화 리뷰는 [performance-optimization-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\performance-optimization-review.md)를 본다.
- 네트워크 세션 recv buffer 리뷰는 [session-recv-buffer-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\session-recv-buffer-review.md)를 본다.
- 네트워크 세션 송신 큐 리뷰는 [session-send-queue-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\session-send-queue-review.md)를 본다.

## 2. 문서 배치 원칙
- 테스트 계획과 실행 이력도 `reviews/` 아래 카테고리 문서로 함께 관리한다.
- 같은 주제는 가능한 한 `통합 리뷰 1개 + 테스트 이력 1개` 조합으로 유지한다.
- 예전 `docs/reviews/mmorpg-v2/`, `docs/testing/mmorpg-v2/` 문서는 현재 카테고리 문서로 흡수하고 제거한다.

## 3. 관리 원칙
- 새 리뷰를 추가할 때는 먼저 기존 통합 문서에 흡수 가능한지 확인한다.
- 테스트 관련 문서는 먼저 해당 카테고리의 `testing-history.md`에 추가 가능한지 확인한다.
- 별도 문서가 꼭 필요한 경우에만 새 파일을 만든다.
