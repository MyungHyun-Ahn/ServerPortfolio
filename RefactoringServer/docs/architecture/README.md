# Architecture 가이드

## 1. 디렉터리 규칙
- `architecture`는 주제별 디렉터리 기준으로 정리한다.
- 각 디렉터리와 문서는 `001_`, `002_` 같은 숫자 prefix를 기본 규칙으로 사용한다.
- 새 구조 문서를 만들기 전에 기존 문서를 확장할 수 있는지 먼저 확인한다.

## 2. 읽는 순서
1. [001_project-overview.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/architecture/001_project/001_project-overview.md)
2. [002_coding-convention.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/architecture/001_project/002_coding-convention.md)
3. [003_cpp-header-pch-convention.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/architecture/001_project/003_cpp-header-pch-convention.md)
4. [001_networklib-overview.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/architecture/002_networklib/001_networklib-overview.md)
5. [001_contents-runtime-overview.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/architecture/003_contentsruntime/001_contents-runtime-overview.md)
6. [001_login-platform-overview.md](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/architecture/004_login-platform/001_login-platform-overview.md)

## 3. 현재 구조
- `001_project`
  - 프로젝트 전체 방향, 코딩 규칙, 헤더/PCH 규칙
- `002_networklib`
  - 패킷, 세션, 송수신, IOCP/RIO 기반 네트워크 코어
- `003_contentsruntime`
  - 콘텐츠 스레드, 세션 라우팅, 콘텐츠 인스턴스 실행 모델
- `004_login-platform`
  - `LoginServer`, `Infra`, `Redis/MySQL`, `ChattingServer LoginAuth` 경계와 인증 흐름

## 4. 관리 원칙
- 새 구조 문서를 추가하면 이 인덱스도 같이 갱신한다.
- 상위 문서는 경계와 책임을 설명하고, 구현 세부는 `current`, `plans`, `reviews`로 내려보낸다.
- 구조 변경이 있으면 계획 문서보다 먼저 아키텍처 문서를 최신 상태로 맞춘다.
