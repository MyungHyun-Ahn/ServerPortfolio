# Docs 인덱스

## 1. 빠른 진입점
- 계획 문서는 [plans/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\README.md)를 본다.
- 아키텍처 문서는 [architecture/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\README.md)를 본다.
- 리뷰 문서는 [reviews/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\README.md)를 본다.

## 2. 읽는 순서 추천
1. [001_project-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\001_project-overview.md)
2. [001_networklib-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\002_networklib\001_networklib-overview.md)
3. [001_contents-runtime-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\003_contentsruntime\001_contents-runtime-overview.md)
4. [README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\README.md)
5. [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)
6. [README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\README.md)

## 3. 디렉터리 역할
- `docs/plans`
  - 구현 전에 방향, 범위, 검증 기준을 정리하는 문서
- `docs/architecture`
  - 현재 구조와 계층 경계를 설명하는 기준 문서
- `docs/reviews`
  - 구현 결과, 검증 결과, 트러블슈팅, 운영 규칙을 정리하는 문서

## 4. 문서 관리 규칙
- 새 계획 문서를 추가하면 `docs/plans/README.md`와 [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)를 같이 갱신한다.
- 새 아키텍처 문서를 추가하면 `docs/architecture/README.md`를 같이 갱신한다.
- 새 리뷰 문서를 추가하면 `docs/reviews/README.md`를 같이 갱신한다.
- 가능한 한 현재 기준 문서를 유지하고, 오래된 중복 초안은 남기지 않는다.
