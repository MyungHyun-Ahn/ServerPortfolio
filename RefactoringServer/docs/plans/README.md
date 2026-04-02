# Plans 가이드

## 1. 목적
- `docs/plans`는 구현 전에 방향, 범위, 검증 기준을 먼저 고정하는 문서 모음이다.
- 코드보다 먼저 판단해야 하는 구조 결정, 단계별 작업 순서, 후속 확인 항목을 기록한다.

## 2. 디렉터리/파일 규칙
- 작업 디렉터리는 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 각 작업 디렉터리 안의 문서도 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 같은 주제의 후속 작업은 가능한 한 기존 번호 디렉터리 안에 이어서 추가한다.
- 같은 숫자 prefix 규칙은 `docs/architecture`, `docs/reviews`에도 동일하게 적용한다.

## 3. 현재 구조
- 전체 진행 상태는 [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)에서 본다.
- [001_foundation](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation)
- [002_legacy-mhlib](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\002_legacy-mhlib)
- [003_networklib-crypto](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\003_networklib-crypto)
- [004_networklib-session](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\004_networklib-session)
- [005_networklib-packet-view](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\005_networklib-packet-view)
- [006_packet-schema-tooling](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling)
- [007_networklib-performance](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance)
- [008_contents-runtime](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime)

## 4. 작성 규칙
- 구현 설명보다 방향과 판단 근거를 먼저 적는다.
- 완료 보고 문서가 아니라, 구현 전에 합의할 작업 문서라는 관점을 유지한다.
- 검증 방식이 중요하면 plan 문서에 명시적으로 남긴다.
- TODO, 후속 확인 항목, 롤백 기준이 있으면 분리해서 적는다.

## 5. 상태판 갱신 규칙
- `plans` 문서를 새로 추가하면 [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)를 반드시 같이 갱신한다.
- 기존 작업의 상태가 바뀌면 상태판도 같은 커밋에 반영한다.
- 상태는 최소한 `완료`, `진행 중`, `추가 확인 필요` 중 하나로 유지한다.

## 6. 패킷 생성 규칙
- `PacketGenerator`는 수동 실행 기준이다.
- 일반 C++ 프로젝트 빌드 중 자동 실행하지 않는다.
- 수동 실행 진입점:
  - [Generate-Packets.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.ps1)
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\Generate-Packets.cmd)

## 7. 콘텐츠 문서 묶음 규칙
- 콘텐츠 스키마, generated packet output, 콘텐츠 런타임 문서는 가능한 한 같은 주제 묶음으로 관리한다.
- 참고 문서:
  - [002_content-directory-layout.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling\002_content-directory-layout.md)

## 8. ContentsRuntime 문서 묶음
- [001_contents-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\001_contents-runtime-architecture.md)
- [002_contents-runtime-instrumentation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\002_contents-runtime-instrumentation.md)
- [003_contents-runtime-lockfree-hot-path.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\003_contents-runtime-lockfree-hot-path.md)
- [004_contents-runtime-lockfree-validation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\004_contents-runtime-lockfree-validation.md)
- [005_contents-runtime-directory-refine.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\005_contents-runtime-directory-refine.md)
- [006_content-bridge-expansion.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\006_content-bridge-expansion.md)
