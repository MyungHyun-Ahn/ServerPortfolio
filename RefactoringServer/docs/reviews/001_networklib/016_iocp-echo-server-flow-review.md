# IOCP EchoServer Flow Review

## 1. 목적
- 현재 `Backend: Iocp` 기준으로 `EchoClient -> EchoServer -> ContentsRuntime -> EchoServer -> EchoClient` 흐름을 호출 스택 중심으로 정리한다.
- 특히 accept 경로는 예전 `accept()` thread가 아니라 현재 구현된 `AcceptEx + IOCP completion` 기준으로 설명한다.

## 2. 대상 파일
- 서버 시작 / application 경계
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
- IOCP backend
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
- IOCP session
  - [FIocpSession.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.h)
  - [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Session\FIocpSession.cpp)
- 콘텐츠 경계
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
  - [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)
- Echo content
  - [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Contents\Echo\FEchoContent.cpp)
- 클라이언트
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)

## 3. 구조 요약
- `EchoClient`는 raw Winsock 기반이지만 packet serialization / framing / cipher는 `NetworkLib`를 쓴다.
- `EchoServer`는 `FEchoApplication`을 통해 transport와 `ContentsRuntime`를 연결한다.
- IOCP backend는 현재 `AcceptEx`를 여러 개 pre-post 하고, accept completion도 worker의 `GetQueuedCompletionStatus()`에서 처리한다.
- send 경로는 현재 `SendPacket(sessionId, FOutgoingContentPacket&&)` 기준이다.
- `NetworkLib` transport header는 애플리케이션 계층에 직접 노출되지 않는다.

## 4. EchoClient에서 Rq를 만드는 흐름
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
- `RunSingleSession(...)` 안에서
  - `FLoginRq` 전송 / `FLoginRp` 수신
  - `FRoomListRq` 전송 / 응답 수신
  - `FRoomEnterRq` 전송 / `FRoomEnterRp` 수신
- bootstrap이 끝난 뒤에만 `EchoRq` 전송 루프로 들어간다.

### 4-4. EchoRq 직렬화와 전송
- `RunSingleSession(...)`
  - `Generated::Echo::FEchoRq requestPacket`
  - `NetworkLib::Packet::Serialization::SerializeContentPacket(requestPacket)`
  - `packetCipher.Encode(...)`
  - `packetFramer.BuildPacket(...)`
  - `SendPacketWithOptionalChunking(...)`

## 5. EchoServer 시작 흐름
### 5-1. backend 선택
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - `FEchoServerConfigLoader::LoadFromFile(...)`
  - `ApplyEchoServerConfigDocument(...)`
  - `FServerFactory::Create(serverConfig.backendKind)`

`Backend: Iocp`면:
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FServerFactory.cpp)
  - `FIocpServer` 생성

### 5-2. Start 호출
- `server->Start(serverConfig, echoApplication)`
- `echoApplication`은 `IApplicationHandler` 구현체다.

## 6. IOCP AcceptEx 호출 스택
### 6-1. Start 내부 초기화
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)
  - `FIocpServer::Start(...)`

주요 순서:
1. `InitializeWinsock()`
2. `CreateIoCompletionPort(INVALID_HANDLE_VALUE, ...)`
3. `OpenListenSocket()`
4. `CreateIoCompletionPort(m_listenSocket, ..., kAcceptCompletionKey, ...)`
5. `LoadAcceptExFunctions()`
6. `InitializeAcceptContexts()`
7. `StartWorkers()`
8. `m_applicationHandler->OnServerStarted(*this)`

### 6-2. AcceptEx pre-post
- `FIocpServer::InitializeAcceptContexts()`
  - `m_acceptContextCount` 계산
  - `SAcceptContext[]` 생성
  - 각 slot마다 `PostAccept(slotIndex)` 호출

- `FIocpServer::PostAccept(slotIndex)`
  - 필요 시 이전 `acceptedSocket` 정리
  - `WSASocketW(...)`로 새 accept socket 생성
  - `m_acceptEx(...)` 호출
  - `WSA_IO_PENDING`이면 정상 경로

즉 현재 IOCP 서버는
- blocking `accept()` thread가 없고
- accept request도 IOCP completion plane으로 들어오도록 pre-post 한다.

### 6-3. AcceptEx completion 처리
- `FIocpServer::WorkerLoop()`
  - `GetQueuedCompletionStatus(...)`
  - `completionKey == kAcceptCompletionKey`면 accept completion branch 진입

주요 순서:
1. `HandleAcceptCompletion(*acceptContext, queuedResult != FALSE, completionError)`
2. 성공 시 `AttachAcceptedSocket(acceptContext.acceptedSocket)`
3. 그 뒤 같은 slot에 `PostAccept(acceptContext->slotIndex)` 재게시

즉 accept도 recv/send와 같은 completion plane에서 돈다.

### 6-4. 현재 accept pool 해석
- `SAcceptContext[]`는 고정 길이 slot pool로 재사용된다.
- 다만 `acceptedSocket`은 slot 안에서 유지되더라도 매 repost마다 새로 만든다.
- 따라서 현재 구조는
  - `accept context slot pool`: 있음
  - `accepted socket reuse`: 없음

### 6-5. AttachAcceptedSocket 호출 스택
- `FIocpServer::AttachAcceptedSocket(clientSocket)`

주요 순서:
1. `setsockopt(SO_UPDATE_ACCEPT_CONTEXT)`
2. 필요 시 `SO_SNDBUF`
3. 비어 있는 session slot 탐색
4. `FIocpSession::Create()`
5. `FIocpSession::Initialize(...)`
6. `CreateIoCompletionPort(clientSocket, m_iocpHandle, ...)`
7. slot attach
8. `m_applicationHandler->OnClientConnected(sessionId)`
9. `PostRecv(*newSessionContext)`

## 7. IOCP recv -> application -> ContentsRuntime
### 7-1. 첫 recv post
- `FIocpServer::PostRecv(FIocpSession&)`
  - `BuildRecvWsabufs(...)`
  - `WSARecv(...)`

### 7-2. worker completion loop
- `FIocpServer::WorkerLoop()`
  - `GetQueuedCompletionStatus(...)`
  - accept completion이면 accept branch
  - 아니면 `FIocpSession::SIoContext` 복원
  - `ioType == Recv`면 recv branch
  - `ioType == Send`면 send branch

### 7-3. recv completion 처리
- recv branch 안에서
  - `CommitRecvBytes(transferredBytes)`
  - `m_packetFramer->TryExtractPacketView(...)`
  - checksum 검증
  - 필요 시 `packetCipher->Decode(...)`
  - `TryParseContentPacketView(...)`
  - `m_applicationHandler->OnPacketReceived(*this, sessionId, contentPacketView)`
  - consumed bytes discard
  - `PostRecv(*sessionContext)`

## 8. EchoServer application dispatch
### 8-1. 연결 직후
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - `FEchoApplication::OnClientConnected(sessionId)`
  - `m_contentRuntime.EnterSession(sessionId, kAuthContentId)`

### 8-2. packet 수신 후
- `FEchoApplication::OnPacketReceived(server, sessionId, packetView)`
  - 필요 시 trace/log
  - `m_contentRuntime.EnqueuePacket(sessionId, packetView.opcode, packetView.payload, packetView.payloadLength)`

## 9. ContentsRuntime -> Echo content
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

주요 순서:
1. `AcquireSession(sessionId)`
2. `packet.MoveBuffer()`로 content payload 획득
3. 필요 시 cipher encode
4. 필요 시 framer `BuildPacketParts(...)`
5. `sessionContext->EnqueueSendBuffer(...)`
6. `PostSend(*sessionContext)`

### 10-4. WSASend post
- `FIocpServer::PostSend(FIocpSession&)`

주요 순서:
1. `TryBeginSend()`
2. `FillSendBatch(...)`
3. `sendContext.Prepare(EIoType::Send, &sessionContext)`
4. `BeginSendIo()`
5. `WSASend(...)`

### 10-5. send completion
- `FIocpServer::WorkerLoop()` send branch
  - `FinishSendIo()`
  - `ReleaseActiveSendBuffers()`
  - `EndSend()`
  - queued send가 남아 있으면 다시 `PostSend(*sessionContext)`

## 11. EchoClient에서 Rp를 받는 흐름
- `RunSingleSession(...)`
  - `ReceiveSinglePacket(...)`
  - `packetFramer.TryExtractPacket(...)`
  - `packetCipher.Decode(...)`
  - `DeserializeContentPacket<Generated::Echo::FEchoRp>(...)`
- 받은 `EchoRp.message`를 검증한 뒤 다음 `EchoRq` 또는 `RoomChangeRq`로 진행한다.

## 12. 현재 판단
- IOCP accept path는 이제 `accept()` thread 기반이 아니라 `AcceptEx + IOCP completion` 기반이다.
- accept context slot pool은 적용되어 있다.
- `accepted socket reuse`는 아직 기본 채택하지 않았고, 현재는 매 repost마다 새 socket을 만든다.
- 현재 판단으로는 socket 재사용은 기본값으로 강제하기보다 별도 실험 옵션 수준이 더 적절하다.
