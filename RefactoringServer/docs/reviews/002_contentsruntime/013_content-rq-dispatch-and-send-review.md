# Content Rq 처리와 Send 흐름 리뷰

## 1. 목적
- 이 문서는 `Content Rq` 패킷이 서버에 들어왔을 때 어떤 경로로 처리되는지 정리한다.
- 특히 `EnqueuePacket`, `TryBeginSend`, `EndSend`가 각각 어느 단계에서 쓰이는지 `EchoRq -> EchoRp`를 기준으로 설명한다.
- 핵심은 `receive 쪽 콘텐츠 큐`와 `send 쪽 네트워크 큐`가 서로 다른 파이프라인이라는 점을 분리해서 이해하는 것이다.

## 2. 한 줄 요약
- `EnqueuePacket`은 **클라이언트가 보낸 Rq를 콘텐츠 스레드로 넘기는 receive-side 큐**다.
- `TryBeginSend / EndSend`는 **서버가 만든 Rp를 실제 소켓으로 내보내는 send-side 제어**다.
- 즉 `EchoRq`는 `NetworkLib -> ApplicationHandler -> ContentsRuntime -> ContentThread`로 들어오고,
  `EchoRp`는 `ContentThread -> ContentsRuntime Bridge -> NetworkLib SendQueue -> WSASend`로 나간다.

## 3. Receive 쪽 흐름
### 3.1 IOCP worker가 content packet을 꺼낸다
- `FIocpServer` worker는 `WSARecv` completion 이후 recv buffer를 파싱해서 content packet view를 만든다.
- 파싱된 packet은 `IApplicationHandler::OnPacketReceived(...)`로 전달된다.

대상 코드:
- [IApplicationHandler.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)

중요한 점:
- `packetView`는 borrowed view다.
- 유효 범위는 콜백 안쪽뿐이다.
- 그래서 콘텐츠 스레드로 넘길 때는 owned payload로 바꿔야 한다.

### 3.2 EchoServer 앱 핸들러가 `EnqueuePacket`을 호출한다
- `FEchoApplication::OnPacketReceived(...)`에서 packet opcode를 보고 trace를 남긴 뒤
  `m_contentRuntime.EnqueuePacket(sessionId, packetView.opcode, packetView.payload, packetView.payloadLength)`를 호출한다.

대상 코드:
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)

이 단계에서의 `EnqueuePacket` 의미:
- 아직 send와 관계없다.
- 현재 세션이 어느 콘텐츠 인스턴스에 붙어 있는지 보고,
  그 콘텐츠 스레드의 work queue에 `FOwnedPacketEnvelope`를 넣는 단계다.

### 3.3 `ContentsRuntime`가 현재 라우팅을 보고 대상 콘텐츠 스레드로 넘긴다
- `FContentRuntime::EnqueuePacket(...)`은
  1. `sessionId -> contentInstanceId -> ownerThread` 라우팅을 조회하고
  2. borrowed payload를 `std::vector<char>`로 복사한 `FOwnedPacketEnvelope`를 만들고
  3. `targetThread->EnqueuePacket(...)`을 호출한다.

대상 코드:
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)

이 단계의 의미:
- `NetworkLib` worker thread에서 직접 게임 로직을 실행하지 않는다.
- 네트워크 스레드는 콘텐츠 스레드 inbox까지 넘기는 역할만 한다.

### 3.4 콘텐츠 스레드가 실제 `Rq`를 처리한다
- `FContentThread::EnqueuePacket(...)`은 lock-free work queue에 `SQueuedWorkItem`을 넣고 worker를 깨운다.
- worker thread는 dequeue 후 `content->OnPacket(...)`을 호출한다.

대상 코드:
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)

즉 `EchoRq`는 결국 `FEchoContent::OnPacket(...)`까지 와서 처리된다.

## 4. `EchoRq -> EchoRp` 처리 흐름
### 4.1 `FEchoContent::OnPacket(...)`
- 현재 room content 인스턴스와 route generation을 검증한 뒤 opcode를 보고 분기한다.
- `Generated::Echo::FEchoRq::kOpcode`면 `HandleEchoRq(...)`로 들어간다.

대상 코드:
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)

### 4.2 `HandleEchoRq(...)`
- payload를 `Generated::Echo::FEchoRq`로 deserialize한다.
- 그 다음 `Generated::Echo::FEchoRp responsePacket;`를 만들고
  `ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket)`를 호출한다.

여기서 중요한 점:
- 콘텐츠 스레드는 `WSASend`를 직접 호출하지 않는다.
- bridge를 통해 `NetworkLib` 쪽 send path로 요청을 넘긴다.

대상 코드:
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)

## 5. Send 쪽 흐름
### 5.1 `SendContentPacket(...)`
- `SendContentPacket(...)`은 packet을 `FPacketWriter`로 serialize하고
  `bridge.SendRaw(sessionId, opcode, buffer, length)`를 호출한다.

대상 코드:
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)

### 5.2 `FContentRuntime::SendRaw(...)`
- 현재 `ContentsRuntime`는 `IContentBridge` 구현체다.
- `SendRaw(...)`는 내부에서 `NetworkLib::IServer*`를 꺼내 `server->Send(...)`를 호출한다.

대상 코드:
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)

즉 콘텐츠 스레드가 만든 `EchoRp`는 여기서부터 `NetworkLib` send path로 내려간다.

### 5.3 `FIocpServer::Send(...)`
- `FIocpServer::Send(...)`는 session을 잡고
  1. content header를 붙이고
  2. 필요하면 cipher/framer를 적용하고
  3. `FSendBuffer`를 만든 뒤
  4. `sessionContext->EnqueueSendBuffer(...)`
  5. `PostSend(*sessionContext)`
  순서로 진행한다.

대상 코드:
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)

여기서의 `EnqueueSendBuffer`는:
- 네트워크 send queue다.
- 앞에서 말한 `ContentsRuntime::EnqueuePacket`과 이름이 비슷하지만 완전히 다른 역할이다.

## 6. `TryBeginSend / EndSend`가 하는 일
### 6.1 `EnqueueSendBuffer(...)`
- send buffer를 lock-free send queue에 넣는다.
- 동시에 `kSendPendingFlag`를 세운다.

대상 코드:
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.cpp)

의미:
- “이 세션에는 아직 보내야 할 버퍼가 있다”는 사실을 남긴다.

### 6.2 `TryBeginSend()`
- `PostSend()`가 send를 시작할 수 있는지 판단할 때 쓴다.
- 이미 `kSendInFlightFlag`가 켜져 있으면 `false`
- 아니면
  - `kSendInFlightFlag`를 켜고
  - `kSendPendingFlag`를 지우고
  - `true`를 반환한다.

대상 코드:
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.cpp)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)

의미:
- 한 세션에 대해 동시에 여러 `WSASend`가 겹치지 않게 한다.
- 그리고 “이번 send 시작 시점까지 들어온 pending”을 내가 가져갔다고 표시한다.

### 6.3 `FillSendBatch(...)`
- `TryBeginSend()`가 성공하면 send queue에서 여러 `FSendBuffer`를 꺼내
  active send buffer 목록과 `WSABUF` 배열을 만든다.
- 그 다음 `WSASend(...)`를 건다.

대상 코드:
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.cpp)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)

### 6.4 `EndSend()`
- send completion이 왔을 때 호출된다.
- `kSendInFlightFlag`를 내리고,
- send 도중 새 enqueue가 있었는지(`kSendPendingFlag`)를 bool로 반환한다.

대상 코드:
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.cpp)

의미:
- “이번 send는 끝났다”
- “그 사이 새 버퍼가 들어왔으니 send를 다시 시작해야 한다”
를 동시에 알려주는 함수다.

### 6.5 completion 이후 재기동
- `FIocpServer`는 send completion 이후
  - `EndSend()`가 `true`이거나
  - queue가 아직 남아 있으면
  다시 `PostSend()`를 호출한다.

대상 코드:
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)

이게 현재 복원한 레거시 send 보장의 핵심이다.

## 7. 함수 호출 순서 요약
`EchoRq -> EchoRp`를 가장 짧게 쓰면 다음 순서다.

1. `WSARecv` completion
2. `TryParseContentPacketView(...)`
3. `IApplicationHandler::OnPacketReceived(...)`
4. `FContentRuntime::EnqueuePacket(...)`
5. `FContentThread::EnqueuePacket(...)`
6. content worker thread dequeue
7. `FEchoContent::OnPacket(...)`
8. `FEchoContent::HandleEchoRq(...)`
9. `ContentsRuntime::Bridge::SendContentPacket(...)`
10. `FContentRuntime::SendRaw(...)`
11. `FIocpServer::Send(...)`
12. `FIocpSession::EnqueueSendBuffer(...)`
13. `FIocpServer::PostSend(...)`
14. `FIocpSession::TryBeginSend()`
15. `FIocpSession::FillSendBatch(...)`
16. `WSASend(...)`
17. send completion
18. `FIocpSession::EndSend()`
19. 필요 시 `PostSend(...)` 재호출

## 8. 결론
- `EnqueuePacket`은 `Rq 처리 시작점`이다.
- `TryBeginSend / EndSend`는 `Rp 송신 직렬화와 재기동 보장`을 담당한다.
- 따라서 `EchoRq`를 받았을 때 서버 동작은
  - 먼저 콘텐츠 런타임 큐로 넘기고
  - 콘텐츠 스레드에서 게임 로직을 실행하고
  - 그 결과 `EchoRp`를 네트워크 send queue에 넣어
  - `TryBeginSend / EndSend`로 안전하게 `WSASend`를 이어가는 구조다.
