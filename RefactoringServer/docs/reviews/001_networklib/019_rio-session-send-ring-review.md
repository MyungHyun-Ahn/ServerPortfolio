# RIO Session Send Ring Review

## 1. 목적
- `RIO` send 경로를 `per-packet send buffer` 방식에서 `session-local send ring` 방식으로 바꾼 구현을 리뷰한다.
- 이번 리뷰의 초점은 correctness, `Direct / OwnerThread` 모드별 ownership, 그리고 현재 테스트 범위에서 드러난 남은 리스크 정리다.

## 2. 범위
검토 대상 파일:
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)

관련 기획서:
- [014_rio-registered-buffer-pool.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\014_rio-registered-buffer-pool.md)
- [015_rio-send-hot-path-overhead-reduction.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\015_rio-send-hot-path-overhead-reduction.md)

## 3. Findings
### 3-1. Blocking finding 없음
- 현재 구현 기준으로는 `blocking` 급 correctness 문제는 찾지 못했다.
- `RIO Direct`, `RIO OwnerThread` 모두 새 `session send ring` 경로로 `60초` 스모크와 `5분` 고부하 런을 통과했다.
- 테스트 로그에서 의도한 fail-fast 조건인 `send stall`, `packet exceeded max send packet size`, `RIOSend failed`도 재현되지 않았다.

### 3-2. 기존 관측성 공백은 후속 작업으로 해소됐다
- 초기 구현 시점에는 `queuedSendBuffers`가 사실상 `OwnerThread` 큐 길이만 반영해서, `Direct` 모드의 실제 ring 압력이 잘 보이지 않는 공백이 있었다.
- 이후 후속 작업으로 세션/서버 통계에 다음 값이 추가됐다.
  - `sendRingUsedBytes`
  - `sendRingInFlightBytes`
  - `maxObservedSendRingUsedBytes`
  - `totalSendRingUsedBytes`
  - `totalSendRingInFlightBytes`
- 따라서 현재는 `Direct` 모드에서도 `queuedSendBuffers=0`과 별개로 실제 ring pressure를 운영 로그에서 해석할 수 있다.

### 3-3. 기존 Direct 예외 경로 lock 범위 우려도 후속 작업으로 정리됐다
- 초기 구현 시점에는 [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp) 의 `AppendPacketToSendRing()`가 `m_sendRingMutex` 안에서 바로 `CloseSession()`까지 호출했다.
- 후속 작업에서 이 경로는 `lock 안에서 실패 판정만 수행 -> lock 밖에서 경고 로그 + CloseSession()` 구조로 정리됐다.
- 이후 `OversizePacket`과 `SendStall`을 강제로 재현해도 세션은 여전히 fail-fast로 닫혔고, lock 범위 축소 후 deadlock이나 이중 close 징후도 보이지 않았다.

### 3-4. OwnerThread 경로는 packet buffer를 raw pointer로 쌓으므로 byte 단위 압력 지표가 있으면 좋다
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp) 의 `EnqueueOwnerSendPacket()`는 `FPacketBuffer*`를 큐에 적재하고 개수만 카운트한다.
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp) 의 `DrainOwnerThreadSendQueue()`가 드레인하기 전까지는 packet buffer가 그대로 살아 있으므로, queue count만으로는 실제 메모리 압력을 충분히 설명하지 못한다.

권장:
- OwnerThread 모드에는 `queuedOwnerSendBytes` 같은 byte 기반 지표를 같이 남기면 튜닝이 쉬워진다.

## 4. 구현 요약
### 4-1. 세션별 send ring
- 각 `FRioSession`은 `64KiB` 크기의 `m_sendRingBuffer`와 `RIO_BUFFERID`를 가진다.
- send ring register는 per-send가 아니라 세션 객체 수명에 붙는다.
- 풀로 돌아갈 때는 ring 상태만 reset하고, `RIODeregisterBuffer`는 서버 종료 시 `ReleaseAllSendRingRegistrations()`에서 일괄 처리한다.

### 4-2. 패킷 크기 / stall 정책
- 최대 packet 크기는 `8KiB`다.
- 이를 넘는 packet은 비정상으로 보고 바로 세션을 닫는다.
- `64KiB` ring이 가득 차서 정상 packet append가 불가능해도 정상 backpressure로 숨기지 않고 `send stall`로 보고 세션을 닫는다.

### 4-3. 세션당 in-flight send 1개
- `FRioSession::TryPrepareNextSend()`는 `m_sendRingInFlightBytes == 0`일 때만 다음 send를 준비한다.
- `HandleSendCompletion()`에서 `CompleteCurrentSend()` 후 남은 데이터가 있으면 `PostSend()`로 다음 send를 이어 건다.
- 즉 세션당 outstanding send는 항상 1개로 유지된다.

### 4-4. 모드별 ownership
- `Direct`
  - 호출 스레드가 `SendPacket()`에서 바로 encode + framing + ring append를 수행한다.
  - 이 경로는 `m_sendRingMutex`로 보호된다.
- `OwnerThread`
  - `SendPacket()`은 `FPacketBuffer*`만 owner queue에 넣는다.
  - 실제 encode + framing + ring append + send submit은 owner worker가 수행한다.
  - 따라서 ring 자체는 단일 스레드가 만져 lock이 필요 없다.

## 5. 검증 결과
### 5-1. 60초 스모크
- `Direct`
  - [client.console.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_smoke\direct\client.console.log)
  - `echo validation succeeded. sessions=250 responses=6687`
- `OwnerThread`
  - [client.console.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_smoke\owner\client.console.log)
  - `echo validation succeeded. sessions=250 responses=6650`
- 두 런 모두 [server.console.err.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_smoke\direct\server.console.err.log), [server.console.err.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_smoke\owner\server.console.err.log) 에 치명 오류가 없었다.

### 5-2. 5분 고부하
조건:
- `250 sessions`
- `holdSeconds=300`
- `interval=0`
- `room-change=90%`
- race / sleep injection 비활성

결과:
- `Direct`
  - [client.console.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_5m\direct\client.console.log)
  - `echo validation succeeded. sessions=250 responses=26667`
- `OwnerThread`
  - [client.console.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_5m\owner\client.console.log)
  - `echo validation succeeded. sessions=250 responses=25701`

추가 확인:
- 두 서버 로그 모두 `send stall`, `packet exceeded max send packet size`, `RIOSend failed`가 검출되지 않았다.
- 이번 조건에선 `Direct`가 `OwnerThread`보다 약간 높은 처리량을 보였다.

### 5-3. 후속 검증: 관측성/예외 경로 정리
- `RIO Direct` 30초 고부하 스모크에서 ring 사용량 통계가 실제로 찍히는 것을 확인했다.
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_114226_ring_obs_smoke\summary.csv)
  - [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_114226_ring_obs_smoke\rio_direct\server.log)
- `Direct` 예외 경로 lock 범위 축소 후에도 정상 경로는 유지됐다.
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_120411_direct_lockscope_smoke_retry\summary.csv)
- 강제 예외 재현도 모두 성공했다.
  - oversize: [forced_rio_direct_oversize_20260407_121031](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_oversize_20260407_121031)
  - send stall: [forced_rio_direct_send_stall_strong_20260407_121147](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_send_stall_strong_20260407_121147)
- 즉 현재 시점에는 초기 리뷰에서 남겨둔 두 우려가 모두 후속 작업으로 해소된 상태다.

## 6. 결론
- 이번 `session send ring` 전환은 현재 테스트 범위에서 correctness와 안정성 기준을 충족한다.
- 특히 `per-send send buffer`, `per-send request context 할당`, `per-send register/deregister` 경로를 걷어내면서도 `Direct / OwnerThread` 양쪽에서 정상 동작을 확인했다.
- 초기 리뷰에서 남겼던 `Direct` ring 관측성 공백과 예외 경로 lock 범위 우려는 후속 작업으로 정리됐다.
- 따라서 다음 우선순위는 기능 수정이 아니라
  - `OwnerThread` byte 단위 backlog 통계 추가
  - `Direct` 미세 최적화와 관측성 기반 해석
  - 필요 시 `OwnerThread` 경량화 재검토
순으로 보는 것이 맞다.
