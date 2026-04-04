# RIO EchoServer Flow Review

## 1. 목적
- 순수 `RIO` backend가 `EchoServer`에서 실제로 어떻게 동작하는지 코드 흐름 기준으로 정리한다.
- 특히 다음 질문에 바로 답할 수 있게 한다.
  - backend 선택은 어디서 되는가
  - accept는 어떤 흐름으로 처리되는가
  - recv completion은 어떻게 `ContentsRuntime`까지 올라가는가
  - `EchoRp`는 어떤 경로로 `RIOSend()`까지 내려가는가

## 2. 큰 구조
핵심 구성은 네 층이다.

1. 설정 / backend 선택
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.cpp)

2. transport backend
- [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)

3. session / RIO 상태
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)

4. application / contents
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.h)
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)

## 3. 시작 흐름
1. `EchoServer.yaml`에서 `Backend: Rio`를 읽는다.
- [EchoServer.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Server\EchoServer.yaml)

2. [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)에서 generated config를 `SServerConfig`로 변환한다.
- `ToBackendKind(...)`
- `serverConfig.backendKind = ToBackendKind(configDocument.EchoServer.Backend);`

3. 같은 파일에서 [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.cpp)로 backend를 만든다.
- `FServerFactory::Create(serverConfig.backendKind)`

4. `Backend: Rio`면 `FRioServer`가 생성된다.

5. `server->Start(serverConfig, echoApplication)`가 호출된다.
- 여기서 `echoApplication`은 `IApplicationHandler` 구현체다.
- transport는 packet을 받고, application handler는 session 이벤트와 packet 이벤트를 받는다.

## 4. FRioServer 시작 흐름
[FRioServer::Start](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)은 아래 순서로 초기화한다.

1. `m_serverConfig`, logger, cipher, framer를 잡는다.
2. session slot / generation 배열을 만든다.
3. `InitializeWinsock()`
4. `LoadRioFunctionTable()`
5. `OpenListenSocket()`
6. `LoadAcceptExFunction()`
7. `StartWorkers()`
8. `AcceptLoop()`를 전용 thread로 시작한다.
9. 마지막에 `m_applicationHandler->OnServerStarted(*this)`를 호출한다.

중요한 점:
- `RIO`는 순수 backend로 구현되어 있다.
- completion notification은 `RIO_EVENT_COMPLETION` 기반이고, `RIO_IOCP_COMPLETION`은 쓰지 않는다.

## 5. worker / CQ 구조
`FRioServer`는 worker마다 아래를 하나씩 가진다.
- `completionEvent`
- `RIO_CQ`
- owner thread
- `activeSessionCount`

관련 코드:
- [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)

의미:
- CQ는 worker별 단일 owner 구조다.
- 한 CQ를 여러 스레드가 같이 dequeue하지 않는다.

## 6. accept 흐름
accept는 [FRioServer::AcceptLoop](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp) 기준으로 이렇게 흐른다.

1. `WSASocketW(..., WSA_FLAG_REGISTERED_IO)`로 client socket을 미리 만든다.
2. `AcceptEx`를 건다.
3. event 기반으로 completion을 기다린다.
4. 완료되면 `setsockopt(... SO_UPDATE_ACCEPT_CONTEXT ...)`를 호출한다.
5. 그 다음 `AttachAcceptedSocket(clientSocket)`으로 넘긴다.

이 구조를 쓰는 이유:
- accepted socket도 `RIO`에 사용할 socket이어야 하므로 `WSA_FLAG_REGISTERED_IO`로 만든다.
- plain `accept()` 대신 `AcceptEx` 경로를 고정해서 등록 I/O용 socket lifecycle을 명확히 한다.

## 7. session attach 흐름
[FRioServer::AttachAcceptedSocket](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)은 아래 순서로 동작한다.

1. `ChooseLeastLoadedWorkerIndex()`로 owner worker를 정한다.
- 현재 정책은 `activeSessionCount` 기반 least-loaded다.

2. 빈 session slot을 먼저 찾는다.
- 이 순서가 중요하다.
- 예전에는 slot 확정 전에 `RIOCreateRequestQueue()`를 시도해서 `10014` 오류가 났다.

3. `FRioSession::Create()`로 session을 만든다.

4. session을 초기화한다.
- `sessionId`
- `slotIndex`
- `generation`
- `ownerWorkerIndex`
- recv ring buffer / staging buffer 크기

5. recv staging buffer를 `RIORegisterBuffer()`로 등록한다.

6. `RIOCreateRequestQueue()`로 session 전용 `RIO_RQ`를 만든다.
- recv CQ와 send CQ는 현재 둘 다 owner worker의 CQ를 사용한다.

7. 마지막에 slot에 attach한다.
- attach가 성공하면 비로소 live session이 된다.

8. `OnClientConnected(sessionId)`를 application handler에 올린다.

9. `PostRecv()`로 첫 `RIOReceive()`를 건다.

## 8. recv 흐름
recv는 `PostRecv -> worker completion -> HandleRecvCompletion`으로 흐른다.

### 8-1. 첫 recv post
[FRioServer::PostRecv](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- recv pending 중복을 막는다.
- staging buffer의 `RIO_BUF`를 세팅한다.
- request queue mutex를 잡고 `RIOReceive()`를 건다.

### 8-2. completion 소비
[FRioServer::WorkerLoop](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- `RIONotify()`
- `WaitForSingleObject(completionEvent, ...)`
- `RIODequeueCompletion()`
- completion 하나씩 `HandleRioCompletion()`으로 넘긴다.

### 8-3. recv completion 처리
[FRioServer::HandleRioCompletion](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- `RequestContext`를 `SRequestContext*`로 복원한다.
- `requestKind == Recv`면 `HandleRecvCompletion()`으로 넘긴다.

[FRioServer::HandleRecvCompletion](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- staging buffer에서 session recv ring buffer로 복사한다.
- framer로 packet을 분리한다.
- checksum 검증을 한다.
- cipher가 있으면 decode한다.
- `TryParseContentPacketView(...)`로 content packet view를 만든다.
- 마지막에 `m_applicationHandler->OnPacketReceived(*this, sessionId, contentPacketView)`를 호출한다.

즉 transport 관점에서 보면:
- `RIOReceive`
- completion dequeue
- framing / decode
- `IApplicationHandler::OnPacketReceived`
까지가 transport 책임이다.

## 9. EchoServer application dispatch 흐름
[Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)의 `FEchoApplication` 기준이다.

### 9-1. 연결 직후
`OnClientConnected(sessionId)`
- `m_contentRuntime.EnterSession(sessionId, kAuthContentId);`
- 즉 새 세션은 먼저 `Auth` content로 들어간다.

### 9-2. packet 수신 시
`OnPacketReceived(server, sessionId, packetView)`
- packet trace가 필요하면 로그를 남긴다.
- 그 다음 `m_contentRuntime.EnqueuePacket(...)`으로 packet을 `ContentsRuntime`에 넘긴다.

여기서 중요한 분리:
- `FRioServer`는 content thread를 직접 모른다.
- `EchoServer` application handler가 `ContentsRuntime`으로 라우팅한다.

## 10. EchoRq 처리 흐름
`EchoRq`는 최종적으로 [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)에서 처리된다.

1. `ContentsRuntime`가 target content thread로 packet을 넘긴다.
2. `FEchoContent::OnPacket(...)`이 호출된다.
3. opcode가 `FEchoRq::kOpcode`면 `HandleEchoRq(...)`로 들어간다.
4. request payload를 deserialize한다.
5. `FEchoRp`를 만든다.
6. `ContentsRuntime::Bridge::SendContentPacket(...)`을 호출한다.

## 11. EchoRp send 흐름
`EchoRp`는 아래 경로로 내려간다.

1. `FEchoContent::HandleEchoRq(...)`
2. `ContentsRuntime::Bridge::SendContentPacket(...)`
3. `FContentRuntime::SendRaw(...)`
4. `IServer::Send(...)`
5. `FRioServer::Send(sessionId, opcode, buffer, length)`

[FRioServer::Send](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp) 내부에서는:
- `AcquireSession(sessionId)`로 live session을 찾는다.
- packet을 framing / encode한다.
- send용 `FPacketBuffer`를 만든다.
- 그 버퍼를 `RIORegisterBuffer()`로 등록한다.
- `RIOSend()`를 건다.

send completion은 다시:
- `WorkerLoop`
- `HandleRioCompletion`
- `HandleSendCompletion`
순서로 처리된다.

`HandleSendCompletion`에서는:
- send queue 통계를 내린다.
- temporary registered buffer를 `RIODeregisterBuffer()`한다.
- `FPacketBuffer`를 pool로 반환한다.

## 12. 현재 주의사항
1. 현재 send buffer는 send마다 `RIORegisterBuffer()` / `RIODeregisterBuffer()`를 한다.
- baseline으로는 단순하고 안전하지만, 이후 성능 최적화 후보다.

2. recv는 staging buffer -> recv ring buffer 복사가 있다.
- zero-copy 최적화 전 단계다.

3. session ownership은 accept 시 결정되고 고정된다.
- migration은 아직 없다.

4. request queue 접근은 현재 lock을 허용한다.
- 이후 hot path 계측 후 lock-free 후보만 분리하는 방향이다.

5. `IOCP + RIO` 하이브리드는 이번 구현에 포함되지 않는다.
- 나중에 별도 backend로 분리하는 게 원칙이다.

## 13. 지금 문서의 결론
- 순수 `RIO` backend는 이제 `EchoServer`에서 실제로 packet 왕복과 `ContentsRuntime` 흐름을 끝까지 태우는 상태다.
- accept, recv, send, content dispatch가 모두 실 구현 기준으로 연결되어 있다.
- 다음 단계는 구조 설명이 아니라 성능 측정과 최적화다.
