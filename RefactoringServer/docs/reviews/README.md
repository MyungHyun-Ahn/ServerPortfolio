# Reviews Guide

## 1. 현재 기준 문서
- 컨테이너 관련 리뷰는 [lock-free-containers-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\containers\lock-free-containers-review.md)를 본다.
- 컨테이너 테스트 이력은 [testing-history.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\containers\testing-history.md)를 본다.
- 진단 모듈 리뷰는 [crash-dump-module-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\diagnostics\crash-dump-module-review.md)를 본다.
- 로깅 관련 리뷰는 [logger-module-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\logging\logger-module-review.md)를 본다.
- 메모리 풀 관련 리뷰는 [memory-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\memory\memory-pool-review.md)를 본다.
- 메모리 풀 테스트 이력은 [testing-history.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\memory\testing-history.md)를 본다.
- 네트워크 패킷 암호화 리뷰는 [packet-cipher-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-cipher-review.md)를 본다.

## 2. 문서 배치 원칙
- 테스트 계획과 실행 이력도 `reviews/` 아래 카테고리 문서로 함께 관리한다.
- 같은 주제는 가능한 한 `통합 리뷰 1개 + 테스트 이력 1개` 조합으로 유지한다.
- 예전 `docs/reviews/mmorpg-v2/`, `docs/testing/mmorpg-v2/` 문서는 현재 카테고리 문서로 흡수하고 제거한다.

## 3. 관리 원칙
- 새 리뷰를 추가할 때는 먼저 기존 통합 문서에 흡수 가능한지 확인한다.
- 테스트 관련 문서는 먼저 해당 카테고리의 `testing-history.md`에 추가 가능한지 확인한다.
- 별도 문서가 꼭 필요한 경우에만 새 파일을 만든다.
