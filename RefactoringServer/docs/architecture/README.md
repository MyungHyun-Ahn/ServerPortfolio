# Architecture Guide

## 1. 읽는 순서
- 프로젝트 전체 방향은 [project-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\project\project-overview.md)를 본다.
- 코드 스타일과 명명 규칙은 [coding-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\project\coding-convention.md)를 본다.
- 새 `NetworkLib`의 역할과 경계는 [networklib-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\networklib\networklib-overview.md)를 본다.
- 백엔드 추상화와 서버 인터페이스는 [backend-abstraction.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\networklib\backend-abstraction.md)를 본다.
- lock-free queue, stack, memory pool 기반은 [lockfree-foundation.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\networklib\lockfree-foundation.md)를 본다.

## 2. 관리 원칙
- 같은 주제의 설계는 가능한 한 한 문서에서 현재 기준을 설명한다.
- 구현 상세가 커지면 하위 주제로 문서를 분리하되, 상위 문서에서 읽는 순서를 먼저 안내한다.
- 현재 기준 문서만 남기고, 흡수된 초안이나 중복 문서는 제거한다.
