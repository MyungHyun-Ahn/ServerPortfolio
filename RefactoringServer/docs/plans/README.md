# 계획 문서 안내

상태: 활성  
정본: 예  
최종 갱신: 2026-04-09  
범위: 계획 문서 인덱스

`docs/plans`는 구현 전에 방향과 경계를 고정하는 설계 문서 모음이다.  
여기서는 현재 기준에서 바로 참고할 가치가 큰 문서를 먼저 보여준다.

## 1. 우선 확인 문서
- 전체 우선순위와 진행 상태: [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)
- `Node.js LoginServer`와 `WinForms` 프로토타입: [001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform\001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md)
- `Node.js LoginServer` 우선 구현과 `WinForms` 외부 인증 전환: [002_nodejs-loginserver-first-and-winforms-auth-integration-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform\002_nodejs-loginserver-first-and-winforms-auth-integration-plan.md)
- `Connector` 라이브러리 경계와 `Redis/MySQL` 정리 방향: [001_connector-library-layout-and-integration-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\013_connector_library\001_connector-library-layout-and-integration-plan.md)
- `NetworkLib` 성능과 RIO send path: [019_networklib-diagnostics-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\019_networklib-diagnostics-runtime-architecture.md)
- `ContentsRuntime` 현재 구조 축: [012_mailbox-owner-transfer-work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\012_mailbox-owner-transfer-work-stealing-and-delegate.md)
- `ChattingServer` 패킷과 흐름 계약: [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)
- `ChattingDummyClient` 부하 구조: [003_chattingdummy-load-test-client-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\003_chattingdummy-load-test-client-plan.md)
- 벤치 실행 구조: [001_powershell-benchmark-runner-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\011_benchmark_runner\001_powershell-benchmark-runner-plan.md)

## 2. 디렉터리 맵
- [001_foundation](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\001_foundation)
- [002_legacy-mhlib](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\002_legacy-mhlib)
- [003_networklib-crypto](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\003_networklib-crypto)
- [004_networklib-session](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\004_networklib-session)
- [005_networklib-packet-view](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\005_networklib-packet-view)
- [006_packet-schema-tooling](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling)
- [007_networklib-performance](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance)
- [008_contents-runtime](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime)
- [009_chatting_server](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server)
- [010_worldserver](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\010_worldserver)
- [011_benchmark_runner](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\011_benchmark_runner)
- [012_login-platform](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform)
- [013_connector_library](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\013_connector_library)

## 3. 사용 원칙
- 최신 구현 기준은 먼저 `docs/current`에서 확인한다.
- 구조 근거나 후속 설계 세부가 필요할 때만 `docs/plans`로 내려온다.
- 새 계획 문서를 추가하면 상태 보드와 관련 `current` 문서도 같이 갱신한다.
