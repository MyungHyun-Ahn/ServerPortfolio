# Docs Guide

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: Top-level docs entry

`docs`는 `RefactoringServer` 문서를 목적별로 빠르게 찾기 위한 허브다.  
모든 문서를 한 번에 펼쳐 보이기보다, 지금 무엇을 보려는지에 따라 먼저 볼 진입점을 나누는 구조로 운영한다.

## 1. Start Here
- 포트폴리오 관점 요약: [portfolio/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\README.md)
- 현재 구현 기준: [current/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\README.md)
- 설계 문서 히스토리: [plans/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\README.md)
- 구현/실험 문서 히스토리: [reviews/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\README.md)
- 구조 설명: [architecture/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\README.md)

## 2. 목적별 진입점
### 포트폴리오로 보여줄 때
1. [001_optimization-index.md](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\001_optimization-index.md)
2. [002_benchmark-index.md](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\002_benchmark-index.md)
3. [001_project-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\001_project-current.md)

### 지금 코드 작업을 이어갈 때
1. [001_project-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\001_project-current.md)
2. [002_networklib-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\002_networklib-current.md)
3. [003_contentsruntime-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\003_contentsruntime-current.md)
4. [004_chatting-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\004_chatting-current.md)
5. [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)

## 3. 디렉터리 역할
- `docs/portfolio`
  포트폴리오용 상위 요약과 대표 index
- `docs/current`
  사람과 AI가 먼저 읽는 최신 기준 문서
- `docs/plans`
  설계 방향과 구현 계획의 히스토리
- `docs/reviews`
  구현 결과, 실험 결과, 문제 분석의 히스토리
- `docs/architecture`
  현재 구조 설명과 모듈 경계 설명

## 4. 운영 규칙
- 새 결론이 나오면 `plans` 또는 `reviews`만 갱신하지 말고 관련 `current` 또는 `portfolio` 문서도 같이 갱신한다.
- 오래된 문서를 바로 지우지 않고 `plans`와 `reviews`에 보관한다.
- README류 문서는 모든 문서를 나열하지 않고, 지금 읽어야 할 대표 문서만 노출한다.
