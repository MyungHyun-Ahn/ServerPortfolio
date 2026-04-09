# Plans Guide

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: Planning history index

`docs/plans`는 구현 전에 정리한 설계 방향과 작업 계획을 모아둔 문서 저장소다.  
여기서는 모든 문서를 자세히 나열하기보다, 지금 기준에서 먼저 볼 대표 계획 문서만 보여준다.

## 1. 현재 대표 계획 문서
- 전체 우선순위와 진행 상태: [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)
- `NetworkLib` 성능과 RIO send path: [019_networklib-diagnostics-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\019_networklib-diagnostics-runtime-architecture.md)
- `ContentsRuntime` 현재 구조 축: [012_mailbox-owner-transfer-work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\012_mailbox-owner-transfer-work-stealing-and-delegate.md)
- `ChattingServer` 패킷/흐름: [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)
- `ChattingDummyClient`와 부하 구조: [003_chattingdummy-load-test-client-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\003_chattingdummy-load-test-client-plan.md)
- `C# ClientNetworkLib` 방향과 재사용 전략: [001_csharp-clientnetworklib-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\001_csharp-clientnetworklib-plan.md)
- `PacketGenerator` C# 출력 세부 계획: [002_packetgenerator-csharp-output-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support\002_packetgenerator-csharp-output-plan.md)
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
- [012_csharp_support](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\012_csharp_support)

## 3. 사용 규칙
- 최신 기준은 먼저 `docs/current`에서 확인한다.
- 왜 이런 구조가 나왔는지 추적할 때만 `plans` 문서로 내려간다.
- 새 plan을 추가하면 상태판과 관련 `current` 문서도 같이 갱신한다.
