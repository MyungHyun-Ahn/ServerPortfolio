# Docs Guide

## 1. 빠른 진입
- 현재 기준 문서: [current/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\README.md)
- 계획 문서: [plans/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\README.md)
- 아키텍처 문서: [architecture/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\README.md)
- 리뷰 문서: [reviews/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\README.md)

## 2. 추천 읽기 순서
1. [001_project-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\001_project-current.md)
2. [002_networklib-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\002_networklib-current.md)
3. [003_contentsruntime-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\003_contentsruntime-current.md)
4. [004_chatting-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\004_chatting-current.md)
5. [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)
6. [099_session-handoff.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\099_session-handoff.md)
7. [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)

## 3. 디렉터리 역할
- `docs/current`
  - 사람과 AI가 먼저 보는 최신 기준 문서
- `docs/plans`
  - 설계 방향, 범위, 구현 계획 히스토리
- `docs/architecture`
  - 현재 구조와 경계 설명
- `docs/reviews`
  - 구현 결과, 실험 결과, 문제 분석 히스토리

## 4. 관리 규칙
- 새로운 결론이 나오면 `plans`나 `reviews`만 갱신하지 말고 관련 `current` 문서도 함께 갱신한다.
- `current` 문서는 긴 히스토리를 복사하지 않고 최신 기준과 핵심 링크만 담는다.
- 상세 근거와 원문 기록은 계속 `plans`와 `reviews`에 남긴다.
