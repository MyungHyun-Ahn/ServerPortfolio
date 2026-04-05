# IOCP EchoServer Flow Review

## 1. 목적
- 현재 기준 `Backend: Iocp`에서 `EchoClient -> EchoServer -> ContentsRuntime -> EchoServer -> EchoClient` 흐름이 어떻게 이어지는지 코드 호출 스택 기준으로 정리한다.
- 특히 다음 항목을 빠르게 확인할 수 있게 하는 것이 목적이다.
  - `EchoClient`가 `Rq`를 직렬화하고 보내는 경로
  - `IOCP` accept / attach / `WSARecv` post 흐름
  - `GetQueuedCompletionStatus` 이후 packet dispatch 흐름
  - `EchoRp`가 `WSASend()`까지 내려가는 경로

## 2. 대상 파일
- 서버 시작 / application 경계
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- IOCP backend
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
- IOCP session
  - [FIocpSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
  - [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.cpp)
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

## 3. 공통 전제
- `EchoClient`는 raw Winsock 기반이지만 packet serialization / framing / cipher는 `NetworkLib`를 사용한다.
- `EchoServer`는 `FEchoApplication`을 통해 transport와 `ContentsRuntime`를 연결한다.
- 현재 send 경계는 `SendPacket(sessionId, FOutgoingContentPacket&&)` 기준이다.
- 앱 계층은 `NetworkLib` transport header를 직접 만지지 않는다.

## 4. EchoClient에서 Rq를 보내는 흐름
### 4-1. 세션 루프
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)
  - `main()`
  - `RunSingleSession(sessionIndex, options, rttMetricsRuntime)`

### 4-2. 연결
- `RunSingleSession(...)`
  - `TryConnectSocket(options, clientSocket, errorMessage)`
- `TryConnectSocket(...)`
  - `socket()`
  - `connect()`
  - 필요 시 `SO_RCVTIMEO`

### 4-3. bootstrap
- `RunSingleSession(...)` 안에서:
  - `FLoginRq` 전송 / `FLoginRp` 수신
  - `FRoomListRq` 수신
  - `FRoomEnterRq` 전송 / `FRoomEnterRp` 수신
- bootstrap이 끝난 뒤에야 `EchoRq` 전송 루프에 들어간다.

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

즉 클라이언트도 지금은:
- 앞쪽에 content header 공간을 가진 writer
- 뒤쪽 body serialize
- helper가 content header finalize
구조를 사용한다.

## 5. EchoServer 시작 흐름
### 5-1. backend 선택
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - `FEchoServerConfigLoader::LoadFromFile(...)`
  - `ApplyEchoServerConfigDocument(...)`
  - `FServerFactory::Create(serverConfig.backendKind)`

`Backend: Iocp`이면:
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.cpp)
  - `FIocpServer` 생성

### 5-2. Start 호출
- `server->Start(serverConfig, echoApplication)`
- `echoApplication`은 `IApplicationHandler` 구현체다.

## 6. IOCP accept 호출 스택
### 6-1. Start 내부 초기화
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
  - `FIocpServer::Start(...)`

주요 순서:
1. `InitializeWinsock()`
2. `CreateIoCompletionPort(INVALID_HANDLE_VALUE, ...)`
3. `OpenListenSocket()`
4. `StartWorkers()`
5. `m_acceptThread = std::thread(&FIocpServer::AcceptLoop, this)`
6. `m_applicationHandler->OnServerStarted(*this)`

### 6-2. accept 루프
- `FIocpServer::AcceptLoop()`

주요 순서:
1. `accept(m_listenSocket, ...)`
2. `AttachAcceptedSocket(clientSocket)`
3. attach 실패 시 `closesocket(clientSocket)`

RIO와 다르게 IOCP는:
- `AcceptEx`가 아니라 `accept()`
- accepted socket을 `CreateIoCompletionPort`로 IOCP에 attach한다.

### 6-3. AttachAcceptedSocket 호출 스택
- `FIocpServer::AttachAcceptedSocket(clientSocket)`

주요 순서:
1. 빈 session slot 탐색
2. `FIocpSession::Create()`
3. `FIocpSession::Initialize(...)`
4. `CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), m_iocpHandle, ...)`
5. slot attach
6. `m_applicationHandler->OnClientConnected(sessionId)`
7. `PostRecv(*newSessionContext)`

즉 IOCP accept 이후 첫 네트워크 작업은:
- session attach
- app 연결 이벤트
- 첫 `WSARecv`
순서다.

## 7. IOCP recv -> EchoServer application -> ContentsRuntime 흐름
### 7-1. 첫 recv post
- `FIocpServer::PostRecv(FIocpSession&)`
  - `BuildRecvWsabufs(...)`
  - `WSARecv(...)`

### 7-2. completion 소비
- `FIocpServer::WorkerLoop()`
  - `GetQueuedCompletionStatus(...)`
  - `FIocpSession::SIoContext` 복원
  - `ioType == Recv`이면 recv branch
  - `ioType == Send`이면 send completion branch

### 7-3. recv completion 처리
- `WorkerLoop()` recv branch 안에서:
  - `CommitRecvBytes(transferredBytes)`
  - `m_packetFramer->TryExtractPacketView(...)`
  - checksum 검증
  - 필요 시 `packetCipher->Decode(...)`
  - `TryParseContentPacketView(...)`
  - `m_applicationHandler->OnPacketReceived(*this, sessionId, contentPacketView)`
  - consumed bytes discard
  - `PostRecv(*sessionContext)` 재호출

즉 IOCP 경로는:
- `WSARecv`
- `GetQueuedCompletionStatus`
- framing / decode
- `OnPacketReceived`
순서다.

## 8. EchoServer application dispatch 흐름
### 8-1. 연결 직후
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - `FEchoApplication::OnClientConnected(sessionId)`
  - `m_contentRuntime.EnterSession(sessionId, kAuthContentId)`

### 8-2. packet 수신 후
- `FEchoApplication::OnPacketReceived(server, sessionId, packetView)`
  - 필요 시 trace/log
  - `m_contentRuntime.EnqueuePacket(sessionId, packetView.opcode, packetView.payload, packetView.payloadLength)`

즉 transport와 contents 사이의 경계는:
- `IApplicationHandler`
- `ContentsRuntime`
로 유지된다.

## 9. ContentsRuntime -> Echo content 흐름
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `FContentRuntime::EnqueuePacket(...)`
  - route lookup
  - target `FContentThread` enqueue

- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)
  - `FEchoContent::OnPacket(...)`
  - opcode switch
  - `HandleEchoRq(...)`
  - `HandleRoomListRq(...)`
  - `HandleRoomChangeRq(...)`

즉 `EchoRq`는 최종적으로:
1. `FIocpServer`
2. `FEchoApplication`
3. `FContentRuntime`
4. `FContentThread`
5. `FEchoContent::HandleEchoRq`
순으로 들어간다.

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

### 10-3. IOCP send 계층
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
  - `FIocpServer::SendPacket(...)`

현재 기준 주요 순서:
1. `AcquireSession(sessionId)`
2. `packet.MoveBuffer()`로 content payload 획득
3. cipher가 있으면 payload encode
4. framer가 있으면 `BuildPacketParts(...)`
5. `sessionContext->EnqueueSendBuffer(...)`
6. `PostSend(*sessionContext)`

### 10-4. 실제 `WSASend()` post
- `FIocpServer::PostSend(FIocpSession&)`

주요 순서:
1. `TryBeginSend()`
2. `FillSendBatch(kMaxSendBatchCount)`
3. `GetSendWsabufs()`
4. `WSASend(...)`

### 10-5. send completion
- 다시 `WorkerLoop()` send branch로 돌아온다.
  - `FinishSendIo()`
  - `ReleaseActiveSendBuffers()`
  - `EndSend()`
  - 필요 시 `PostSend()` 재호출

즉 `EchoRp`는 최종적으로:
- `FEchoContent::HandleEchoRq`
- `SendContentPacket`
- `FContentRuntime::SendPacket`
- `FIocpServer::SendPacket`
- `PostSend`
- `WSASend`
순으로 내려간다.

## 11. EchoClient에서 Rp를 받는 흐름
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)
  - `RunSingleSession(...)`
  - `tryReceiveNextContentPacket(...)`

주요 순서:
1. `recv()` 또는 timeout wait
2. `packetFramer.TryExtractPacketView(...)`
3. checksum 검증
4. `packetCipher.Decode(...)`
5. `TryParseContentPacketView(...)`
6. `DeserializeContentPacket(contentPacketView, responsePacket)`

즉 EchoClient 수신은:
- transport header 해석
- content header 해석
- generated packet deserialize
순으로 끝난다.

## 12. 현재 기준에서 중요하게 바뀐 점
1. `SendRaw(opcode, buffer, length)`가 아니라 `SendPacket(FOutgoingContentPacket&&)` 경로다.
2. `SContentHeader`는 앱이 직접 쓰지 않는다.
3. `BuildOutgoingContentPacket(...)`가 content payload를 완성한다.
4. 이 변경으로 `SContentHeader + body` 재복사가 줄었다.

## 13. 현재 해석
- `IOCP` 경로는 현재 `EchoServer`에서 accept / recv / send / contents dispatch 전체 흐름이 현재 `SendPacket` 기준으로 정리 가능한 상태다.
- `RIO`와 비교했을 때 큰 차이는:
  - accept: `accept()` vs `AcceptEx`
  - completion: `GetQueuedCompletionStatus` vs `RIODequeueCompletion`
  - send submit: `WSASend` vs `RIOSend`
- 그 외 상위 application / contents dispatch 구조는 거의 동일하다.
