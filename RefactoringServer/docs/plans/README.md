# Plans 가이드

## 1. 네이밍 규칙
- `plans`는 작업 주제별 디렉터리로 정리한다.
- 각 작업 디렉터리는 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 디렉터리 안의 문서도 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 같은 주제의 후속 작업은 가능한 한 같은 번호 디렉터리 안에 이어서 쌓는다.
- 이 숫자 prefix 정책은 `docs/architecture`, `docs/reviews`에도 동일하게 적용한다.

## 2. 현재 구조
- 전체 진행 상태는 [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)를 함께 본다.
- [001_foundation](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation)
- [002_legacy-mhlib](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\002_legacy-mhlib)
- [003_networklib-crypto](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\003_networklib-crypto)
- [004_networklib-session](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\004_networklib-session)
- [005_networklib-packet-view](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\005_networklib-packet-view)
- [006_packet-schema-tooling](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling)
- [007_networklib-performance](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance)
- [008_contents-runtime](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime)

## 3. ContentsRuntime 문서 묶음
- [001_contents-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\001_contents-runtime-architecture.md)
- [002_contents-runtime-instrumentation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\002_contents-runtime-instrumentation.md)
- [003_contents-runtime-lockfree-hot-path.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\003_contents-runtime-lockfree-hot-path.md)
- [004_contents-runtime-lockfree-validation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\004_contents-runtime-lockfree-validation.md)

## 4. 작성 규칙
- 구현 전에 방향과 범위를 먼저 고정하는 문서로 쓴다.
- 날짜 중심 이름보다 작업 중심 이름을 우선한다.
- 실행 정책이나 검증 기준이 중요하면 plan에 명시적으로 남긴다.
- 새 `plans` 문서를 추가하면 [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)도 반드시 함께 갱신한다.

## 5. 패킷 생성 정책
- `PacketGenerator`는 수동 실행 기준이다.
- 일반 C++ 프로젝트 빌드 중 자동 실행하지 않는다.
- 수동 실행 진입점:
  - [Generate-Packets.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.ps1)
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.cmd)

## 6. 콘텐츠 문서 묶음 규칙
- 콘텐츠 스키마와 generated packet output은 콘텐츠 카테고리 디렉터리 기준으로 함께 묶는다.
- 참고 문서:
  - [002_content-directory-layout.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling\002_content-directory-layout.md)
