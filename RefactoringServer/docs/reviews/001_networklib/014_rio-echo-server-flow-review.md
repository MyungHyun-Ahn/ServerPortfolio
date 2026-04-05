# RIO EchoServer Flow Review

## 1. 목적
- 현재 기준 `Backend: Rio`에서 `EchoClient -> EchoServer -> ContentsRuntime -> EchoServer -> EchoClient` 흐름이 실제로 어떻게 이어지는지 코드 호출 스택 기준으로 정리한다.
- 특히 다음 질문에 바로 답할 수 있게 하는 것이 목적이다.
  - `EchoClient`가 `Rq`를 만들고 보내는 경로는 어디인가
  - `EchoServer`의 `AcceptEx` 기반 accept 흐름은 어떻게 이어지는가
  - `RIO` recv completion이 어떻게 `ContentsRuntime`까지 올라가는가
  - `EchoRp`가 어떤 경로로 `RIOSend()`까지 내려가는가

## 2. 대상 파일
- 서버 시작 / application 경계
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- RIO backend
  - [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.h)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
- RIO session
  - [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.h)
  - [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FRioSession.cpp)
- 콘텐츠 런타임
  - [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.h)
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
  - [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)
- 패킷 직렬화 / send packet 경계
  - [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketSerialization.h)
  - [FPacketWriter.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketWriter.h)
- Echo content
  - [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)
- 클라이언트
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)

## 3. 현재 구조 요약
- `EchoClient`는 여전히 raw Winsock으로 동작하지만, 패킷 직렬화/프레이밍은 `NetworkLib`를 사용한다.
- `EchoServer`는 `IApplicationHandler` 구현체인 `FEchoApplication`을 통해 transport와 `ContentsRuntime`를 연결한다.
- `RIO` backend는 `AcceptEx + WSA_FLAG_REGISTERED_IO + RIO_EVENT_COMPLETION` 조합으로 동작한다.
- 현재 send 경계는 예전 `SendRaw(opcode, buffer, length)`가 아니라 `SendPacket(sessionId, FOutgoingContentPacket&&)` 기준이다.
- 이 변경으로 애플리케이션 계층은 `NetworkLib` transport header를 직접 다루지 않는다.

## 4. EchoClient에서 Rq를 보내는 흐름
### 4-1. 세션 루프 진입
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)
  - `main()`
  - `RunSingleSession(sessionIndex, options, rttMetricsRuntime)`

### 4-2. 연결
- `RunSingleSession(...)`
  - `TryConnectSocket(options, clientSocket, errorMessage)`
- `TryConnectSocket(...)`
  - `socket()`
  - `connect()`
  - 필요 시 `SO_RCVTIMEO` 설정

### 4-3. 로그인 / 룸 진입 bootstrap
- 같은 `RunSingleSession(...)` 안에서 순서대로 진행된다.
  - `FLoginRq` 전송
  - `FRoomListRq` 수신
  - `FRoomEnterRq` 전송
  - `FRoomEnterRp` 수신
- 정상 bootstrap 후에만 `EchoRq` 루프로 들어간다.

### 4-4. EchoRq 직렬화와 전송
- `RunSingleSession(...)`
  - `Generated::Echo::FEchoRq requestPacket`
  - `NetworkLib::Packet::Serialization::SerializeContentPacket(requestPacket)`
  - `packetCipher.Encode(...)`
  - `packetFramer.BuildPacket(...)`
  - `SendPacketWithOptionalChunking(...)`

현재 `SerializeContentPacket(...)` 내부는:
- [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\Serialization\FPacketSerialization.h)
  - `BuildOutgoingContentPacket(packet).MoveBuffer()`

즉 클라이언트도 현재는:
- writer가 앞쪽에 `SContentHeader` 공간을 잡고
- body를 뒤에 serialize한 뒤
- `NetworkLib` 쪽 helper가 content header를 채워서 완성된 content payload를 만든다.

## 5. EchoServer 시작 흐름
### 5-1. config와 backend 선택
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - `FEchoServerConfigLoader::LoadFromFile(...)`
  - `ApplyEchoServerConfigDocument(...)`
  - `FServerFactory::Create(serverConfig.backendKind)`

`Backend: Rio`이면:
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.cpp)
  - `FRioServer` 생성

### 5-2. Start 호출
- `server->Start(serverConfig, echoApplication)`
- `echoApplication`은 `IApplicationHandler` 구현체다.

## 6. RIO accept 호출 스택
### 6-1. Start 내부 초기화
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
  - `FRioServer::Start(...)`

주요 순서:
1. `InitializeWinsock()`
2. `LoadRioFunctionTable()`
3. `OpenListenSocket()`
4. `LoadAcceptExFunction()`
5. `StartWorkers()`
6. `m_acceptThread = std::thread(&FRioServer::AcceptLoop, this)`
7. `m_applicationHandler->OnServerStarted(*this)`

### 6-2. AcceptEx 루프
- `FRioServer::AcceptLoop()`

실제 흐름:
1. `WSASocketW(..., WSA_FLAG_REGISTERED_IO)`로 accepted socket 후보 생성
2. `AcceptEx(...)` 호출
3. event wait
4. 완료 후 `SO_UPDATE_ACCEPT_CONTEXT`
5. `AttachAcceptedSocket(clientSocket)`

즉 RIO 경로의 accept는:
- `accept()`가 아니라 `AcceptEx`
- accepted socket도 미리 `WSA_FLAG_REGISTERED_IO`로 만든다.

### 6-3. AttachAcceptedSocket 호출 스택
- `FRioServer::AttachAcceptedSocket(clientSocket)`

주요 순서:
1. `ChooseLeastLoadedWorkerIndex()`
2. 빈 session slot 탐색
3. `FRioSession::Create()`
4. `FRioSession::Initialize(...)`
5. recv staging buffer용 `RIORegisterBuffer(...)`
6. `RIOCreateRequestQueue(...)`
7. slot attach
8. `m_applicationHandler->OnClientConnected(sessionId)`
9. `PostRecv(*newSessionContext)`

현재 owner worker 정책은:
- `activeSessionCount` 기반 least-loaded
- accept 시 owner를 정하고 세션은 그 worker에 고정된다.

## 7. RIO recv -> EchoServer application -> ContentsRuntime 흐름
### 7-1. 첫 recv post
- `FRioServer::PostRecv(FRioSession&)`
  - recv pending 상태 체크
  - staging buffer를 `RIO_BUF`로 설정
  - `RIOReceive(...)`

### 7-2. CQ 소비
- `FRioServer::WorkerLoop(workerIndex)`

주요 순서:
1. `DrainSendCommands(workerIndex)`
2. `RIONotify(worker.completionQueue)`
3. `WaitForSingleObject(worker.completionEvent, ...)`
4. `RIODequeueCompletion(...)`
5. 각 completion마다 `HandleRioCompletion(...)`

### 7-3. recv completion 처리
- `FRioServer::HandleRioCompletion(...)`
  - `requestKind == Recv`면 `HandleRecvCompletion(...)`

- `FRioServer::HandleRecvCompletion(...)`
  - staging buffer -> session recv ring buffer 복사
  - `m_packetFramer->TryExtractPacketView(...)`
  - checksum 검증
  - 필요 시 `packetCipher->Decode(...)`
  - `TryParseContentPacketView(...)`
  - `m_applicationHandler->OnPacketReceived(*this, sessionId, contentPacketView)`

여기까지가 transport 계층 책임이다.

## 8. EchoServer application dispatch 흐름
### 8-1. 연결 직후
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - `FEchoApplication::OnClientConnected(sessionId)`
  - `m_contentRuntime.EnterSession(sessionId, kAuthContentId)`

즉 새 세션은 먼저 `Auth` content로 들어간다.

### 8-2. packet 수신 후
- `FEchoApplication::OnPacketReceived(server, sessionId, packetView)`
  - 필요 시 trace/log
  - `m_contentRuntime.EnqueuePacket(sessionId, packetView.opcode, packetView.payload, packetView.payloadLength)`

즉 `FRioServer`는 contents thread를 직접 호출하지 않고:
- `IApplicationHandler`
- `ContentsRuntime`
순서로 넘긴다.

## 9. ContentsRuntime -> Echo content 흐름
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `FContentRuntime::EnqueuePacket(...)`
  - route lookup
  - target `FContentThread`에 envelope enqueue

- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)
  - `FEchoContent::OnPacket(...)`
  - opcode switch
  - `HandleEchoRq(...)`
  - `HandleRoomListRq(...)`
  - `HandleRoomChangeRq(...)`

즉 `EchoRq`는 최종적으로:
1. `FRioServer`
2. `FEchoApplication`
3. `FContentRuntime`
4. `FContentThread`
5. `FEchoContent::HandleEchoRq`
순서로 들어간다.

## 10. EchoRp send 호출 스택
### 10-1. content 계층
- `FEchoContent::HandleEchoRq(...)`
  - `Generated::Echo::FEchoRp responsePacket`
  - `ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket)`

### 10-2. bridge / runtime 계층
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)
  - `SendContentPacket(...)`
  - `NetworkLib::Packet::Serialization::BuildOutgoingContentPacket(packet)`
  - `bridge.SendPacket(sessionId, outgoingPacket)`

- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `FContentRuntime::SendPacket(...)`
  - `server->SendPacket(sessionId, std::move(packet))`

### 10-3. RIO send 계층
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp)
  - `FRioServer::SendPacket(...)`

현재 기준 주요 순서:
1. `AcquireSession(sessionId)`
2. `packet.ReleaseBuffer()`
3. framer가 있으면 `BuildPacket(...)`으로 transport packet 생성
4. 현재 mode가 `Direct`면 `SubmitSendDirect(...)`
5. `SubmitSendDirect(...)`
   - `RIORegisterBuffer(...)`
   - `RIOSend(...)`

즉 현재 기본 `Rio Direct` 흐름에서는:
- contents thread가 만든 outgoing packet
- `FRioServer::SendPacket`
- `SubmitSendDirect`
- `RIOSend`
순서로 간다.

## 11. EchoClient에서 Rp를 받는 흐름
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)
  - `RunSingleSession(...)`
  - `tryReceiveNextContentPacket(...)`

주요 순서:
1. `recv()` 또는 timeout 기반 wait
2. `packetFramer.TryExtractPacketView(...)`
3. checksum 검증
4. `packetCipher.Decode(...)`
5. `TryParseContentPacketView(...)`
6. `DeserializeContentPacket(contentPacketView, responsePacket)`

즉 EchoClient 쪽 응답 수신은:
- transport header 제거
- content header 제거
- generated packet deserialize
순서다.

## 12. 현재 기준에서 중요하게 바뀐 점
1. 예전 `SendRaw(opcode, buffer, length)` 경로가 아니라 `SendPacket(FOutgoingContentPacket&&)` 경로다.
2. `SContentHeader`는 앱 계층이 직접 쓰지 않는다.
3. `BuildOutgoingContentPacket(...)`가 front headroom을 이용해 content payload를 완성한다.
4. 이 변경으로 `SContentHeader + body` 재복사가 줄었다.

## 13. 현재 해석
- `RIO` backend는 이제 stub이 아니라, `EchoServer`에서 accept/recv/send/contents dispatch 전체 흐름이 실제로 연결된 상태다.
- `EchoClient`와의 end-to-end 흐름도 현재 `SendPacket` 경로 기준으로 정리 가능하다.
- 이후 성능 최적화는 이 구조 위에서:
  - broadcast fan-out
  - registered buffer 비용 최적화
  - owner-thread send 추가 실험
순서로 보는 게 맞다.
