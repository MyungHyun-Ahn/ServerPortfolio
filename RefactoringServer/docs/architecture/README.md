# Architecture 가이드

## 1. 디렉터리 규칙
- `architecture`는 주제별 디렉터리 기준으로 정리한다.
- 각 디렉터리는 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 각 문서도 `001_`, `002_` 같은 숫자 prefix로 시작한다.
- 새 구조 문서를 만들기 전에 기존 문서를 확장할 수 있는지 먼저 확인한다.

## 2. 읽는 순서
1. [001_project-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\001_project-overview.md)
2. [002_coding-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\002_coding-convention.md)
3. [003_cpp-header-pch-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\003_cpp-header-pch-convention.md)
4. [001_networklib-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\002_networklib\001_networklib-overview.md)
5. [001_contents-runtime-overview.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\003_contentsruntime\001_contents-runtime-overview.md)

## 3. 현재 구조
- `001_project`
  - 프로젝트 전반 방향, 코딩 컨벤션, 헤더/PCH 규칙
- `002_networklib`
  - 소켓, 세션, 패킷, IOCP 기반 네트워크 코어
- `003_contentsruntime`
  - 콘텐츠 스레드, 라우팅, 멀티 콘텐츠/멀티 인스턴스 실행 모델

## 4. 관리 원칙
- 새 구조 문서를 추가하면 이 인덱스를 같이 갱신한다.
- 상위 문서는 경계와 책임을 설명하고, 구현 세부는 하위 문서나 리뷰 문서로 내린다.
- 구조 변경이 있으면 계획 문서보다 먼저 아키텍처 문서를 최신 상태로 맞춘다.
