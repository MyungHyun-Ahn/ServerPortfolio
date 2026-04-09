# 002 NetworkLib Current

Status: Active  
Canonical: Yes  
Last Updated: 2026-04-08  
Scope: NetworkLib current structure and active focus

## 1. 현재 기준
- 핵심 네트워크 라이브러리는 [NetworkLib](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib)다.
- 공용 기반 유틸리티는 [Foundation](D:\Project\ServerPortfolio\RefactoringServer\Libraries\Foundation)에서 제공한다.
- 현재 성능 비교의 중심 backend는 `IOCP`와 `RIO`다.

## 2. 현재 구조 포인트
- 서버 공통 모니터링 집계는 `NetworkLib/Diagnostics`의 `FServerMonitoringRuntime` 기준으로 모으는 방향이다.
- RIO 전용 고빈도 계측은 `FRioSendMetricsRuntime` 기준으로 분리했다.
- TLS shard 수집 기반은 `Foundation/Diagnostics/Tls`의 `FTlsCollectorRuntime` 패턴을 사용한다.

## 3. 현재 해석 기준
- `EchoServer` 벤치는 transport microbenchmark 성격이 강하다.
- `ChattingServer` 벤치는 content + room fan-out + dummy client 처리까지 포함한 end-to-end 성격이다.
- 그래서 `Echo`와 `Chatting`의 backend 순위를 그대로 같은 의미로 해석하지 않는다.

## 4. 현재 관심사
- `RIO Direct`와 `RIO OwnerThread` 차이를 `prepare 비용`, `lock 비용`, `cross-thread send ring touch`로 분리 계측 중이다.
- `Direct`의 병목은 단순 lock 경합만이 아니라 send ring 상태를 서로 다른 스레드가 번갈아 만지는 cache ping-pong 가능성도 같이 본다.
- 다음 최적화는 `RIO Direct` send hot path 정리와 계측 기반 검증이다.

## 5. 관련 핵심 문서
- [019_networklib-diagnostics-runtime-architecture.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\019_networklib-diagnostics-runtime-architecture.md)
- [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
- [023_rio-direct-cross-thread-sendring-cache-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\023_rio-direct-cross-thread-sendring-cache-review.md)
- [027_echo-rio-cache-pingpong-windowsserver-2h-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\027_echo-rio-cache-pingpong-windowsserver-2h-review.md)
- [024_tls-collector-usage-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\024_tls-collector-usage-review.md)
- [025_packet-structure-overview-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\025_packet-structure-overview-review.md)
