# 계획 문서 안내

Status: Active
Canonical: Yes
Last Updated: 2026-04-09
Scope: 계획 문서 인덱스

`docs/plans`는 구현 전에 방향과 경계를 고정하는 설계 문서를 모아둔 곳이다.
여기서는 모든 문서를 세세하게 나열하기보다, 지금 바로 참고할 가치가 큰 문서부터 앞에 둔다.

## 1. 우선 확인 문서
- 전체 우선순위와 진행 상태: [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)
- 로그인 플랫폼 1차 구조: [001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform\001_nodejs-loginserver-and-winforms-chatting-prototype-plan.md)
- 로그인 서버 우선 구축 후 WinForms 인증 연동: [002_nodejs-loginserver-first-and-winforms-auth-integration-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform\002_nodejs-loginserver-first-and-winforms-auth-integration-plan.md)
- Connector 경계와 Redis/MySQL 연동: [001_connector-library-layout-and-integration-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\013_connector_library\001_connector-library-layout-and-integration-plan.md)
- NetworkLib monitoring/runtime 구조: [019_networklib-diagnostics-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\019_networklib-diagnostics-runtime-architecture.md)
- ContentsRuntime 현재 구조 축: [012_mailbox-owner-transfer-work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\012_mailbox-owner-transfer-work-stealing-and-delegate.md)
- Chatting packet/flow: [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)
- ChattingDummy 구조: [003_chattingdummy-load-test-client-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\003_chattingdummy-load-test-client-plan.md)
- C# ClientNetworkLib 방향: [001_csharp-clientnetworklib-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\001_csharp-clientnetworklib-plan.md)
- PacketGenerator C# 출력 계획: [002_packetgenerator-csharp-output-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\002_packetgenerator-csharp-output-plan.md)
- 벤치 러너 구조: [001_powershell-benchmark-runner-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\011_benchmark_runner\001_powershell-benchmark-runner-plan.md)

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
- [012_csharp_support](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support)
- [012_login-platform](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_login-platform)
- [013_connector_library](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\013_connector_library)

## 3. 사용 규칙
- 최신 구현 기준은 먼저 `docs/current`에서 확인한다.
- 왜 이런 구조가 나왔는지 추적이 필요할 때만 `docs/plans`로 내려간다.
- 새로운 plan을 추가하면 `000_plan-status-board.md`와 관련 `current` 문서를 같이 갱신한다.
- `012_*`처럼 숫자 prefix가 겹치는 디렉터리는 실제 폴더명을 기준으로 구분한다.
