# RIO Direct Failure Path Lock Scope Reduction

## 1. 목적
- `RIO Direct`의 비정상 경로에서 `sendRingMutex`를 쥔 채 `CloseSession()`까지 들어가는 구조를 정리한다.
- 정상 경로 성능보다도, 예외 경로의 lock 범위를 줄여 코드 해석성과 안전성을 높이는 것이 목표다.

## 2. 배경
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp) 의 `AppendPacketToSendRing()`는 `Direct` 모드에서 `m_sendRingMutex`를 잡고
  - encode
  - framing
  - ring append
  - oversize / ring full fail-fast 판정
을 수행한다.
- 현재는 `packet > 8KiB` 또는 `ring full`일 때 lock 안에서 바로 `CloseSession()`을 호출한다.
- 정책상 fail-fast 자체는 맞지만, 요청 큐/소켓 정리까지 임계구역에 들어가 예외 경로 해석이 불필요하게 무겁다.

## 3. 목표
- `sendRingMutex` 안에서는 ring 상태 판정과 실패 마킹까지만 수행한다.
- 실제 `CloseSession()` 호출은 lock 밖으로 뺀다.
- 현재의 fail-fast 정책은 유지한다.
  - `packet > 8KiB` => 비정상
  - `64KiB ring full` => `send stall` 비정상

## 4. 범위
대상 파일:
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- 필요 시 [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
- 필요 시 [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)

비범위:
- `Direct`를 lock-free로 바꾸는 작업
- `OwnerThread` 경로 변경
- send ring 크기/정책 변경

## 5. 설계 방향
### 5-1. 실패 이유를 먼저 확정
- `AppendPacketToSendRing()` 내부에서 실패 이유를 enum 또는 local state로 분리한다.
  - `None`
  - `OversizePacket`
  - `SendStall`
  - 필요 시 `BuildPacketPartsFailed`

### 5-2. lock 안 / lock 밖 분리
- lock 안:
  - packet size 판정
  - ring append 시도
  - 실패 이유 기록
- lock 밖:
  - 경고 로그 출력
  - `CloseSession()` 호출

### 5-3. 정책 유지
- lock 범위를 줄여도 정책은 그대로 유지한다.
- 즉 retry/backpressure를 새로 만들지 않고, 기존의 fail-fast는 그대로 둔다.

## 6. 구현 순서
1. `AppendPacketToSendRing()`의 실패 경로를 local result enum으로 분리
2. `sendRingMutex` 구간 밖으로 `CloseSession()` 이동
3. 로그도 가능하면 lock 밖에서 최종 출력
4. `RIO Direct` 스모크와 강제 예외 재현으로 fail-fast 유지 여부 확인

## 7. 검증 기준 및 결과
### 7-1. 검증 기준
- 빌드 성공
- 정상 경로 throughput/RTT 회귀 없음
- oversize / ring full 강제 재현 시
  - 세션은 여전히 fail-fast로 닫힘
  - `CloseSession()`이 lock 밖에서 호출됨
- deadlock, 재진입, 이중 close 징후 없음

### 7-2. 구현 및 검증 결과
- 구현 완료.
- `AppendPacketToSendRing()`은 이제 `sendRingMutex` 안에서는 실패 이유만 판정하고, 실제 경고 로그 출력과 `CloseSession()` 호출은 lock 밖에서 수행한다.
- 전체 `Debug x64` 빌드 성공.
- `Rio Direct` 30초 고부하 재시도 런 성공.
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_120411_direct_lockscope_smoke_retry\summary.csv)
  - `responses=1010880`
  - `AvgSendTPS=16860.5`
  - `EchoAvgMs=7.181`
- `OversizePacket` 강제 재현 성공.
  - 결과 폴더: [forced_rio_direct_oversize_20260407_121031](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_oversize_20260407_121031)
  - 확인 로그: [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_oversize_20260407_121031\server.log)
  - 핵심 로그: `RIO send rejected because packet exceeded max send packet size`
- `SendStall` 강제 재현 성공.
  - 결과 폴더: [forced_rio_direct_send_stall_strong_20260407_121147](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_send_stall_strong_20260407_121147)
  - 확인 로그: [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_send_stall_strong_20260407_121147\server.log)
  - 핵심 로그: `RIO send stall detected because session send ring was full`
  - 함께 기록된 관측성 값:
    - `ringBytes=65536`
    - `packetBytes=7018`
    - `usedBytes=63162`
    - `freeBytes=2374`
    - `inFlightBytes=14036`
- 첫 번째 약한 `send stall` 시나리오에서는 재현되지 않았고, 더 강한 조건에서 정상적으로 fail-fast 경로를 탔다.

## 8. 기대 효과
- 예외 경로의 lock 범위를 줄여 코드 이해가 쉬워진다.
- 이후 관측성/진단 로그를 넣을 때도 lock 구간을 덜 오염시킨다.
- 정상 경로 최적화와 분리된, 안전한 정리 작업으로 진행할 수 있다.
