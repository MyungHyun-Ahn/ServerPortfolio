# Architecture 가이드

## 1. 디렉터리 규칙
- `architecture`는 주제 디렉터리 기준으로 정리한다.
- 새 디렉터리는 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 각 디렉터리 안의 문서도 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 기존 문서는 무리하게 이름만 바꾸기보다, 현재 기준 문서를 우선 정리한다.

## 2. 읽는 순서
- 프로젝트 전체 방향은 [001_project-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\001_project-overview.md)를 본다.
- 코딩 규칙은 [002_coding-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\002_coding-convention.md)를 본다.
- 헤더/PCH 규칙은 [003_cpp-header-pch-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\003_cpp-header-pch-convention.md)를 본다.
- `NetworkLib` 구조는 [001_networklib-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\002_networklib\001_networklib-overview.md)를 본다.
- 백엔드 추상화는 [002_backend-abstraction.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\002_networklib\002_backend-abstraction.md)를 본다.
- lock-free 기반 설계는 [003_lockfree-foundation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\002_networklib\003_lockfree-foundation.md)를 본다.
- `ContentsRuntime` 경계와 처리 흐름은 [001_contents-runtime-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\003_contentsruntime\001_contents-runtime-overview.md)를 본다.

## 3. 현재 구조
- `001_project`
  - 프로젝트 전체 방향, 코딩 컨벤션, 헤더/PCH 규칙
- `002_networklib`
  - 네트워크 코어 역할, 백엔드 추상화, lock-free 기반 설계
- `003_contentsruntime`
  - `ContentsRuntime` 역할, `NetworkLib`와의 경계, 콘텐츠 전이/라우팅 구조

## 4. 관리 원칙
- 같은 주제의 설명은 가능한 한 기준 문서에서 먼저 설명한다.
- 구현 상세가 커지면 하위 문서로 분리하되, 상위 문서에서 읽는 순서를 안내한다.
- 오래된 초안이나 중복 문서는 남기기보다 현재 기준 문서에 흡수한다.
