# Architecture 가이드

## 0. 네이밍 규칙
- `architecture`는 주제 디렉터리 기준으로 정리한다.
- 새로 만드는 디렉터리는 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 디렉터리 안의 새 문서도 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 기존 문서는 한 번에 전부 이름을 바꾸기보다, 현재 기준 문서를 우선 정리하고 필요한 범위부터 prefix 체계로 맞춘다.

## 1. 읽는 순서
- 프로젝트 전체 방향은 [001_project-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\001_project-overview.md)를 본다.
- 코드 스타일과 문서 규칙은 [002_coding-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\002_coding-convention.md)를 본다.
- 헤더/PCH 정책은 [003_cpp-header-pch-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\003_cpp-header-pch-convention.md)를 본다.
- `NetworkLib` 역할과 경계는 [001_networklib-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\002_networklib\001_networklib-overview.md)를 본다.
- 백엔드 추상화는 [002_backend-abstraction.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\002_networklib\002_backend-abstraction.md)를 본다.
- lock-free 기반 설계는 [003_lockfree-foundation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\002_networklib\003_lockfree-foundation.md)를 본다.
- `ContentsRuntime` 경계와 처리 흐름은 [001_contents-runtime-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\003_contentsruntime\001_contents-runtime-overview.md)를 본다.

## 2. 현재 구조
- `001_project`
  - 프로젝트 전반 방향, 코딩 컨벤션, 헤더/PCH 규칙
- `002_networklib`
  - 네트워크 코어 역할, 백엔드 추상화, lock-free 기반 설계
- `003_contentsruntime`
  - 콘텐츠 런타임 역할, `NetworkLib`와의 경계, 콘텐츠 전이/라우팅 구조

## 3. 관리 원칙
- 같은 주제의 설계는 가능한 한 한 문서에서 현재 기준을 설명한다.
- 구현 상세가 커지면 하위 주제로 문서를 분리하되, 상위 문서에서 먼저 읽는 순서를 안내한다.
- 이미 끝난 초안이나 흡수된 문서는 남겨두지 않고, 현재 기준 문서만 유지한다.
