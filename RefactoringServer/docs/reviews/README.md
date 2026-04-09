# Reviews Guide

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-09  
Scope: Review history index

`docs/reviews`는 구현 결과, 실험 결과, 병목 분석, 회고 문서를 보관하는 곳이다.  
빠르게 현재 상태를 파악하려면 먼저 [docs/current](D:\Project\ServerPortfolio\RefactoringServer\docs\current\README.md)와 [docs/portfolio](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\README.md)를 보고, 세부 근거가 필요할 때 이 디렉터리로 내려오면 된다.

## 1. 먼저 볼 리뷰
- 패킷 구조 요약: [025_packet-structure-overview-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\025_packet-structure-overview-review.md)
- `EchoServer` Windows Server 2시간 4모드 결과: [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
- `ChattingServer` Windows Server 1시간 4모드 결과: [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)
- `RIO Direct` send ring 병목 가설: [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)
- `EchoServer` 2시간 cache ping-pong 검증: [027_echo-rio-cache-pingpong-windowsserver-2h-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\027_echo-rio-cache-pingpong-windowsserver-2h-review.md)
- `WinForms + ClientNetworkLib.CSharp` 런타임 스모크 검증: [028_chatting-winforms-csharp-clientnetworklib-smoke-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\028_chatting-winforms-csharp-clientnetworklib-smoke-review.md)
- TLS collector 사용 정리: [024_tls-collector-usage-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\024_tls-collector-usage-review.md)

## 2. 리뷰 디렉터리
- `NetworkLib`: [001_networklib](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib)
- `ContentsRuntime`: [002_contentsruntime](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime)
- `Memory`: [003_memory](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory)
- `Containers`: [004_containers](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers)
- `Diagnostics`: [005_diagnostics](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\005_diagnostics)
- `Logging`: [006_logging](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\006_logging)
- `LoginServer`: [007_loginserver](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\007_loginserver)

## 3. 사용 지침
- 포트폴리오 관점의 요약은 먼저 [docs/portfolio/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\README.md)에서 본다.
- 현재 구현 기준은 먼저 [docs/current/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\current\README.md)에서 본다.
- 실험 조건, 장애 추적, 병목 근거가 필요할 때만 `reviews` 원문으로 내려간다.
