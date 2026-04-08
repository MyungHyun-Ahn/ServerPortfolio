# RIO Direct Cross-Thread SendRing Cache Review

## 1. 목적
- `EchoServer` 기준에서 `RIO Direct`가 `RIO OwnerThread`보다 불리하게 나온 원인을 정리한다.
- 이번 문서의 핵심 가설은 `Direct`의 주원인이 단순 `sendRingMutex` 대기보다, `content worker`와 `RIO owner worker`가 같은 session send ring 상태를 번갈아 만지면서 발생하는 cache 무효화와 cache line ping-pong이라는 것이다.
- 이 문서는 확정 결론이 아니라 현재 코드와 측정 결과를 바탕으로 한 `우선 가설 정리 문서`다.

## 2. 배경
- 관련 결과 정리:
  - [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
- 관련 구현 문서:
  - [014_rio-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\014_rio-echo-server-flow-review.md)
  - [015_rio-send-dispatch-mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\015_rio-send-dispatch-mode-review.md)
  - [019_rio-session-send-ring-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\019_rio-session-send-ring-review.md)

이번에 문제를 본 조건:
- `EchoServer`
- `PayloadSize = 16`
- `SessionCount = 250`
- `HoldSeconds = 7200`
- `IntervalMs = 0`
- `WorkerThreadCount = 4`
- 서버와 클라이언트를 같은 머신에서 동시 실행

## 3. 관찰
- `Windows Server 4코어`의 `EchoServer 2시간 4모드` 결과에서는 `RIO OwnerThread`가 `RIO Direct`보다 처리량이 크게 높고 CPU와 평균 RTT도 더 좋았다.
- 반면 `EchoServer` 설정 자체는 `sendThreadCount = 1`, `responsesPerThread = 1`이라서, 같은 세션에 대해 여러 content thread가 동시에 응답을 쏘는 강한 producer 경쟁은 크지 않다.

근거:
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp#L293)
- [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)

즉 이번 케이스는 "`동시 producer가 많아서 Direct lock 경합이 심했다`"로만 설명하기 어렵다.

## 4. 현재 코드 경로
### 4-1. Echo 응답은 content worker에서 바로 `server->SendPacket()`으로 내려간다
- `FEchoContent`는 기본 설정에서 echo 요청 1개당 응답 1개를 바로 보낸다.
  - [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp#L293)
- `ContentsRuntime`는 이 응답을 곧바로 transport의 `SendPacket()`으로 전달한다.
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp#L654)

즉 `Direct` 모드의 send submit 진입 스레드는 contents worker 쪽이다.

### 4-2. `Direct`는 content worker가 session send ring을 직접 만진다
- `FRioServer::SendPacket()`에서 `Direct`면 바로 `AppendPacketToSendRing(..., true)` 후 `PostSend()`를 수행한다.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L232)
- 이때 `AppendPacketToSendRing()`은 `sendRingMutex` 안에서
  - cipher encode
  - checksum 계산
  - framing
  - send ring append
를 수행한다.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L868)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L931)
- 이어서 `PostSend()`도 `Direct`면 다시 `sendRingMutex`를 잡고 `TryPrepareNextSend()`를 호출한다.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L997)

### 4-3. send completion은 RIO owner worker가 처리한다
- RIO completion은 worker loop에서 처리된다.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L1349)
- send completion이 오면 `HandleSendCompletion()`에서 `Direct` 모드는 다시 `sendRingMutex`를 잡고 `CompleteCurrentSend()`를 수행한다.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L1484)

즉 `Direct`는
- send submit: contents worker
- send completion: RIO owner worker
가 같은 session send ring 상태를 번갈아 만진다.

### 4-4. `OwnerThread`는 ring 조작을 owner worker로 모은다
- `OwnerThread`는 `SendPacket()` 시점에 ring을 직접 만지지 않고, lock-free queue에 packet만 적재한다.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L232)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L1066)
- 이후 owner worker가 send command를 drain하면서 ring append와 `PostSend()`를 처리한다.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L763)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L800)
- 이 경로에서는 `AppendPacketToSendRing(..., false)`를 사용하므로 send ring은 실질적으로 단일 owner thread가 만진다.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L805)

## 5. 핵심 가설
### 5-1. `EchoServer`에서는 강한 mutex 경쟁보다 cross-thread ping-pong이 더 유력하다
- 현재 `EchoServer` 기본 경로에서는 한 요청당 한 응답이라 같은 세션에 대해 여러 content thread가 동시에 `SendPacket()`을 때리는 구조가 아니다.
- 따라서 `sendRingMutex`가 여러 producer 사이에서 오래 block되는 형태의 경합은 이번 워크로드에서 주원인일 가능성이 낮다.
- 하지만 `Direct`는 contents worker와 owner worker가 같은 session send ring 상태를 교대로 갱신한다.

특히 다음 상태가 자주 바뀐다.
- `m_sendRingReadOffset`
- `m_sendRingWriteOffset`
- `m_sendRingUsedBytes`
- `m_sendRingInFlightBytes`
- `m_sendRequestContext`
- `m_sendRingBuffer`의 실제 payload 영역

근거:
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h#L145)

이 구조면
- contents worker가 append/prepare를 위해 ring 상태를 write
- owner worker가 completion 시 ring 상태를 write
- 다시 contents worker가 다음 send를 위해 same cache line을 write
하는 식으로 cache line ownership이 계속 이동한다.

이것이 loopback echo처럼 요청/응답 회전이 매우 빠른 환경에서는 mutex 대기보다 더 큰 비용으로 보일 수 있다.

### 5-2. `Direct`는 임계구역이 길어서 ping-pong 영향이 더 커진다
- `sendRingMutex` 안에서 offset 갱신만 하는 것이 아니라 encode, checksum, framing, append까지 모두 수행한다.
- 그러면 lock hold time도 길어지고, 같은 cache line을 붙잡고 있는 시간도 늘어난다.
- 경쟁 스레드 수가 많지 않아도, 두 스레드 사이에서 ownership 이전 비용이 반복되면 누적 손실이 커질 수 있다.

### 5-3. `OwnerThread`는 ring locality를 얻는 대신 handoff 비용을 낸다
- `OwnerThread`는 contents worker가 직접 ring을 만지지 않는다.
- 대신 lock-free queue enqueue와 send command enqueue가 들어간다.
- 이 구조는 handoff 비용은 있지만, session send ring 자체는 owner worker 한 곳에 묶인다.

즉 `EchoServer` 같은 작은 payload, 빠른 turn-around, 같은 머신 loopback 조건에서는:
- `Direct`의 cross-thread ring ping-pong 비용
- `OwnerThread`의 handoff 비용
중 전자가 더 커져서 `OwnerThread`가 유리했을 가능성이 높다.

## 6. 왜 ChattingServer와 다른가
- `ChattingServer`는 broadcast, room routing, contents runtime queue, dummy client event backlog가 같이 섞인 end-to-end workload다.
- 이 환경에서는 owner handoff 비용, owner queue backlog, contents scheduling 차이가 더 크게 드러날 수 있다.
- 반면 이번 `EchoServer`는 거의 `small packet echo hot path`에 가까워서 ring locality 차이가 더 직접적으로 드러난다.

즉:
- `EchoServer` 결과를 transport microbenchmark로 보고
- `ChattingServer` 결과를 contents end-to-end benchmark로 따로 해석해야 한다.

## 7. 현재 판단
- 현재 코드와 결과를 함께 보면, `EchoServer`에서 `Direct`가 `OwnerThread`보다 느린 이유를 `sendRingMutex`의 강한 대기 경합 하나로 설명하는 것은 부족하다.
- 오히려 더 설득력 있는 가설은:
  - `Direct`에서 contents worker와 owner worker가 같은 session send ring 상태를 번갈아 조작하고
  - 그 과정에서 cache invalidation / cache line ping-pong이 누적되며
  - 거기에 긴 임계구역과 잦은 lock/unlock, per-packet submit 패턴이 합쳐졌다는 것이다.

## 8. 확인이 필요한 계측
- `sendRingMutex wait time`
- `sendRingMutex hold time`
- `Direct`에서 session별 send submit 스레드 분포
- `RIOSend calls/sec`
- `avg bytes per RIOSend`
- send completion 이후 다음 `PostSend()`까지의 지연
- session send ring 관련 필드의 cache locality를 확인할 수 있는 ETW / sampling profiler 데이터

## 9. 다음 액션
- `Direct`에서 `encode / checksum / framing`을 lock 밖으로 빼는 실험
- `Append + TryPrepareNextSend`를 한 번의 lock 구간으로 합치는 실험
- completion path에서 `CompleteCurrentSend + TryPrepareNextSend`를 한 번의 lock 구간으로 합치는 실험
- 위 변경 전후로 `EchoServer` 같은 same-machine loopback 조건에서 재측정
