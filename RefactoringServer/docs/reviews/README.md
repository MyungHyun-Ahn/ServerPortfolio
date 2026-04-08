# Reviews Guide

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: Review history index

`docs/reviews`는 구현 결과, 실험 결과, 병목 분석의 원문 저장소다.  
포트폴리오나 빠른 복기에서는 모든 리뷰를 순서대로 읽기보다, 대표 요약 문서부터 보는 것을 기준으로 둔다.

## 1. 먼저 볼 리뷰
- 패킷 구조 요약: [025_packet-structure-overview-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\025_packet-structure-overview-review.md)
- `EchoServer` Windows Server 2시간 4모드 결과: [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
- `ChattingServer` Windows Server 1시간 4모드 결과: [026_chattingserver-windowsserver-1h-random-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\026_chattingserver-windowsserver-1h-random-4mode-review.md)
- `RIO Direct` send ring 병목 분석: [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)
- TLS collector 사용 정리: [024_tls-collector-usage-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\024_tls-collector-usage-review.md)

## 2. 리뷰 디렉터리
- `NetworkLib`: [001_networklib](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib)
- `ContentsRuntime`: [002_contentsruntime](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime)
- `Memory`: [003_memory](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory)
- `Containers`: [004_containers](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers)
- `Diagnostics`: [005_diagnostics](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\005_diagnostics)
- `Logging`: [006_logging](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\006_logging)
- `LoginServer`: [007_loginserver](/e:/Procademy/myPortfolio/ServerPortfolio/RefactoringServer/docs/reviews/007_loginserver)

## 3. 사용 원칙
- 포트폴리오 관점 요약은 먼저 [portfolio/README.md](D:\Project\ServerPortfolio\RefactoringServer\docs\portfolio\README.md)에서 본다.
- 현재 구현 기준은 먼저 `docs/current`에서 본다.
- 실험 조건, 장애 추적, 세부 근거가 필요할 때만 `reviews` 원문으로 내려온다.
