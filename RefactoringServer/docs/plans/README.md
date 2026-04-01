# Plans Guide

## 1. 기본 규칙
- `plans`는 큰 작업 단위별 디렉터리로 나눈다.
- 큰 작업 디렉터리 이름은 `001_`, `002_`처럼 앞에 순번을 붙인다.
- 각 디렉터리 안의 문서도 `001_`, `002_`처럼 순번을 붙여 읽는 순서가 바로 보이게 한다.

## 2. 현재 구조
- [001_foundation](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation)
  - 공용 기반 모듈과 계층 분리 관련 계획
- [002_legacy-mhlib](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\002_legacy-mhlib)
  - 레거시 MHLib 자산 재사용 계획
- [003_networklib-crypto](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\003_networklib-crypto)
  - `NetworkLib` 암호화 및 프레이밍 관련 계획

## 3. 문서 작성 원칙
- 새 기획서는 먼저 어느 큰 작업에 속하는지 결정한다.
- 같은 작업의 하위 단계면 기존 디렉터리 안에 다음 번호로 추가한다.
- 완전히 새로운 큰 작업이면 새 디렉터리를 만들고 다음 번호를 부여한다.
- 파일명은 날짜 대신 역할이 드러나게 짓는다.

## 4. 예시
- `004_packet-pipeline/001_packet-header.md`
- `004_packet-pipeline/002_default-framer.md`
- `004_packet-pipeline/003_send-recv-integration.md`
