# Plans Guide

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: Planning history index

`docs/plans`는 구현 전에 정리한 설계 방향과 작업 계획의 원문 저장소다.  
여기서는 모든 문서를 자세히 펼치기보다, 지금 기준에서 중요한 대표 문서만 먼저 보여준다.

## 1. 현재 대표 계획 문서
- 전체 우선순위와 진행 상태: [000_plan-status-board.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\000_plan-status-board.md)
- `NetworkLib` 성능과 RIO send path: [019_networklib-diagnostics-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\019_networklib-diagnostics-runtime-architecture.md)
- `ContentsRuntime` 현재 구조 축: [012_mailbox-owner-transfer-work-stealing-and-delegate.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\008_contents-runtime\012_mailbox-owner-transfer-work-stealing-and-delegate.md)
- `ChattingServer` 패킷/흐름: [002_chattingserver-packet-and-flow-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\002_chattingserver-packet-and-flow-plan.md)
- `ChattingDummyClient`와 부하 구조: [003_chattingdummy-load-test-client-plan.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\009_chatting_server\003_chattingdummy-load-test-client-plan.md)
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

## 3. 사용 원칙
- 최신 기준은 먼저 `docs/current`에서 확인한다.
- 세부 설계 근거가 필요할 때만 `plans` 원문으로 내려온다.
- 새 plan을 추가하면 상태판과 관련 current 문서를 같이 갱신한다.
