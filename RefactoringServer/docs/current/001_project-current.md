# 001 Project Current

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: Project-wide working conventions

## 1. 작업 기준
- 현재 작업 기준 루트는 `RefactoringServer`다.
- `RefactoringServer` 밖의 내용은 레거시 프로젝트로 간주한다.
- 새 작업은 가능한 한 `RefactoringServer` 내부 구조와 문서 기준을 따른다.

## 2. 현재 디렉터리 구조
- 라이브러리 프로젝트: [Libraries](D:\Project\ServerPortfolio\RefactoringServer\Libraries)
  - [Foundation](D:\Project\ServerPortfolio\RefactoringServer\Libraries\Foundation)
  - [NetworkLib](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib)
  - [ContentsRuntime](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime)
  - [ClientNetworkLib](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ClientNetworkLib)
- Echo 실행 프로젝트: [Echo](D:\Project\ServerPortfolio\RefactoringServer\Echo)
- Chatting 실행 프로젝트: [Chatting](D:\Project\ServerPortfolio\RefactoringServer\Chatting)
- 스모크/테스트 실행 프로젝트: [SmokeTests](D:\Project\ServerPortfolio\RefactoringServer\SmokeTests)
- 도구 프로젝트: [Tools](D:\Project\ServerPortfolio\RefactoringServer\Tools)

## 3. 스크립트 규칙
- 실험과 반복 실행은 [scripts/bench](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench) 기준으로 통일한다.
- packet/config/codegen 생성 스크립트는 [scripts/generate](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate) 아래에 둔다.
- `Run-*`, `Start-*` 같은 루트 스크립트는 더 이상 늘리지 않는다.

## 4. 출력 규칙
- 프로젝트 빌드 산출물은 [Out](D:\Project\ServerPortfolio\RefactoringServer\Out) 아래 `Out/<ProjectName>/`에 둔다.
- 테스트/벤치 결과는 `Out/bench/`에 둔다.
- 자세한 규칙은 [Out/README.md](D:\Project\ServerPortfolio\RefactoringServer\Out\README.md)를 따른다.

## 5. 먼저 볼 문서
- [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)
- [002_networklib-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\002_networklib-current.md)
- [003_contentsruntime-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\003_contentsruntime-current.md)
- [004_chatting-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\004_chatting-current.md)
- [005_benchmark-current.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\005_benchmark-current.md)
