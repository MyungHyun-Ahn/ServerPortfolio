# IOCP EchoServer Flow Review

## 1. 紐⑹쟻
- ?꾩옱 `Backend: Iocp` 湲곗??쇰줈 `EchoClient -> EchoServer -> ContentsRuntime -> EchoServer -> EchoClient` ?먮쫫???몄텧 ?ㅽ깮 以묒떖?쇰줈 ?뺣━?쒕떎.
- ?뱁엳 accept 寃쎈줈???덉쟾 `accept()` thread媛 ?꾨땲???꾩옱 援ы쁽??`AcceptEx + IOCP completion` 湲곗??쇰줈 ?ㅻ챸?쒕떎.

## 2. ????뚯씪
- ?쒕쾭 ?쒖옉 / application 寃쎄퀎
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
- IOCP backend
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)
- IOCP session
  - [FIocpSession.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.h)
  - [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.cpp)
- 肄섑뀗痢?寃쎄퀎
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)
  - [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Bridge\IContentBridge.h)
- Echo content
  - [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp)
- ?대씪?댁뼵??  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoClient\Main.cpp)

## 3. 援ъ“ ?붿빟
- `EchoClient`??raw Winsock 湲곕컲?댁?留?packet serialization / framing / cipher??`NetworkLib`瑜??대떎.
- `EchoServer`??`FEchoApplication`???듯빐 transport? `ContentsRuntime`瑜??곌껐?쒕떎.
- IOCP backend???꾩옱 `AcceptEx`瑜??щ윭 媛?pre-post ?섍퀬, accept completion??worker??`GetQueuedCompletionStatus()`?먯꽌 泥섎━?쒕떎.
- send 寃쎈줈???꾩옱 `SendPacket(sessionId, FOutgoingContentPacket&&)` 湲곗??대떎.
- `NetworkLib` transport header???좏뵆由ъ??댁뀡 怨꾩링??吏곸젒 ?몄텧?섏? ?딅뒗??

## 4. EchoClient?먯꽌 Rq瑜?留뚮뱶???먮쫫
### 4-1. ?몄뀡 猷⑦봽
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoClient\Main.cpp)
  - `main()`
  - `RunSingleSession(sessionIndex, options, rttMetricsRuntime)`

### 4-2. ?곌껐
- `RunSingleSession(...)`
  - `TryConnectSocket(options, clientSocket, errorMessage)`
- `TryConnectSocket(...)`
  - `socket()`
  - `connect()`
  - ?꾩슂 ??`SO_RCVTIMEO`

### 4-3. bootstrap
- `RunSingleSession(...)` ?덉뿉??  - `FLoginRq` ?꾩넚 / `FLoginRp` ?섏떊
  - `FRoomListRq` ?꾩넚 / ?묐떟 ?섏떊
  - `FRoomEnterRq` ?꾩넚 / `FRoomEnterRp` ?섏떊
- bootstrap???앸궃 ?ㅼ뿉留?`EchoRq` ?꾩넚 猷⑦봽濡??ㅼ뼱媛꾨떎.

### 4-4. EchoRq 吏곷젹?붿? ?꾩넚
- `RunSingleSession(...)`
  - `Generated::Echo::FEchoRq requestPacket`
  - `NetworkLib::Packet::Serialization::SerializeContentPacket(requestPacket)`
  - `packetCipher.Encode(...)`
  - `packetFramer.BuildPacket(...)`
  - `SendPacketWithOptionalChunking(...)`

## 5. EchoServer ?쒖옉 ?먮쫫
### 5-1. backend ?좏깮
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
  - `FEchoServerConfigLoader::LoadFromFile(...)`
  - `ApplyEchoServerConfigDocument(...)`
  - `FServerFactory::Create(serverConfig.backendKind)`

`Backend: Iocp`硫?
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FServerFactory.cpp)
  - `FIocpServer` ?앹꽦

### 5-2. Start ?몄텧
- `server->Start(serverConfig, echoApplication)`
- `echoApplication`? `IApplicationHandler` 援ы쁽泥대떎.

## 6. IOCP AcceptEx ?몄텧 ?ㅽ깮
### 6-1. Start ?대? 珥덇린??- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)
  - `FIocpServer::Start(...)`

二쇱슂 ?쒖꽌:
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
  - `m_acceptContextCount` 怨꾩궛
  - `SAcceptContext[]` ?앹꽦
  - 媛?slot留덈떎 `PostAccept(slotIndex)` ?몄텧

- `FIocpServer::PostAccept(slotIndex)`
  - ?꾩슂 ???댁쟾 `acceptedSocket` ?뺣━
  - `WSASocketW(...)`濡???accept socket ?앹꽦
  - `m_acceptEx(...)` ?몄텧
  - `WSA_IO_PENDING`?대㈃ ?뺤긽 寃쎈줈

利??꾩옱 IOCP ?쒕쾭??- blocking `accept()` thread媛 ?녾퀬
- accept request??IOCP completion plane?쇰줈 ?ㅼ뼱?ㅻ룄濡?pre-post ?쒕떎.

### 6-3. AcceptEx completion 泥섎━
- `FIocpServer::WorkerLoop()`
  - `GetQueuedCompletionStatus(...)`
  - `completionKey == kAcceptCompletionKey`硫?accept completion branch 吏꾩엯

二쇱슂 ?쒖꽌:
1. `HandleAcceptCompletion(*acceptContext, queuedResult != FALSE, completionError)`
2. ?깃났 ??`AttachAcceptedSocket(acceptContext.acceptedSocket)`
3. 洹???媛숈? slot??`PostAccept(acceptContext->slotIndex)` ?ш쾶??
利?accept??recv/send? 媛숈? completion plane?먯꽌 ?덈떎.

### 6-4. ?꾩옱 accept pool ?댁꽍
- `SAcceptContext[]`??怨좎젙 湲몄씠 slot pool濡??ъ궗?⑸맂??
- ?ㅻ쭔 `acceptedSocket`? slot ?덉뿉???좎??섎뜑?쇰룄 留?repost留덈떎 ?덈줈 留뚮뱺??
- ?곕씪???꾩옱 援ъ“??  - `accept context slot pool`: ?덉쓬
  - `accepted socket reuse`: ?놁쓬

### 6-5. AttachAcceptedSocket ?몄텧 ?ㅽ깮
- `FIocpServer::AttachAcceptedSocket(clientSocket)`

二쇱슂 ?쒖꽌:
1. `setsockopt(SO_UPDATE_ACCEPT_CONTEXT)`
2. ?꾩슂 ??`SO_SNDBUF`
3. 鍮꾩뼱 ?덈뒗 session slot ?먯깋
4. `FIocpSession::Create()`
5. `FIocpSession::Initialize(...)`
6. `CreateIoCompletionPort(clientSocket, m_iocpHandle, ...)`
7. slot attach
8. `m_applicationHandler->OnClientConnected(sessionId)`
9. `PostRecv(*newSessionContext)`

## 7. IOCP recv -> application -> ContentsRuntime
### 7-1. 泥?recv post
- `FIocpServer::PostRecv(FIocpSession&)`
  - `BuildRecvWsabufs(...)`
  - `WSARecv(...)`

### 7-2. worker completion loop
- `FIocpServer::WorkerLoop()`
  - `GetQueuedCompletionStatus(...)`
  - accept completion?대㈃ accept branch
  - ?꾨땲硫?`FIocpSession::SIoContext` 蹂듭썝
  - `ioType == Recv`硫?recv branch
  - `ioType == Send`硫?send branch

### 7-3. recv completion 泥섎━
- recv branch ?덉뿉??  - `CommitRecvBytes(transferredBytes)`
  - `m_packetFramer->TryExtractPacketView(...)`
  - checksum 寃利?  - ?꾩슂 ??`packetCipher->Decode(...)`
  - `TryParseContentPacketView(...)`
  - `m_applicationHandler->OnPacketReceived(*this, sessionId, contentPacketView)`
  - consumed bytes discard
  - `PostRecv(*sessionContext)`

## 8. EchoServer application dispatch
### 8-1. ?곌껐 吏곹썑
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
  - `FEchoApplication::OnClientConnected(sessionId)`
  - `m_contentRuntime.EnterSession(sessionId, kAuthContentId)`

### 8-2. packet ?섏떊 ??- `FEchoApplication::OnPacketReceived(server, sessionId, packetView)`
  - ?꾩슂 ??trace/log
  - `m_contentRuntime.EnqueuePacket(sessionId, packetView.opcode, packetView.payload, packetView.payloadLength)`

## 9. ContentsRuntime -> Echo content
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `FContentRuntime::EnqueuePacket(...)`
  - route lookup
  - target `FContentThread` enqueue

- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp)
  - `FEchoContent::OnPacket(...)`
  - opcode switch
  - `HandleEchoRq(...)`
  - `HandleRoomListRq(...)`
  - `HandleRoomChangeRq(...)`

## 10. EchoRp send ?몄텧 ?ㅽ깮
### 10-1. content 怨꾩링
- `FEchoContent::HandleEchoRq(...)`
  - `Generated::Echo::FEchoRp responsePacket`
  - `ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket)`

### 10-2. bridge / runtime 怨꾩링
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Bridge\IContentBridge.h)
  - `SendContentPacket(...)`
  - `NetworkLib::Packet::Serialization::BuildOutgoingContentPacket(packet)`
  - `bridge.SendPacket(sessionId, outgoingPacket)`

- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `FContentRuntime::SendPacket(...)`
  - `server->SendPacket(sessionId, std::move(packet))`

### 10-3. IOCP send 怨꾩링
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)
  - `FIocpServer::SendPacket(...)`

二쇱슂 ?쒖꽌:
1. `AcquireSession(sessionId)`
2. `packet.MoveBuffer()`濡?content payload ?띾뱷
3. ?꾩슂 ??cipher encode
4. ?꾩슂 ??framer `BuildPacketParts(...)`
5. `sessionContext->EnqueueSendBuffer(...)`
6. `PostSend(*sessionContext)`

### 10-4. WSASend post
- `FIocpServer::PostSend(FIocpSession&)`

二쇱슂 ?쒖꽌:
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
  - queued send媛 ?⑥븘 ?덉쑝硫??ㅼ떆 `PostSend(*sessionContext)`

## 11. EchoClient?먯꽌 Rp瑜?諛쏅뒗 ?먮쫫
- `RunSingleSession(...)`
  - `ReceiveSinglePacket(...)`
  - `packetFramer.TryExtractPacket(...)`
  - `packetCipher.Decode(...)`
  - `DeserializeContentPacket<Generated::Echo::FEchoRp>(...)`
- 諛쏆? `EchoRp.message`瑜?寃利앺븳 ???ㅼ쓬 `EchoRq` ?먮뒗 `RoomChangeRq`濡?吏꾪뻾?쒕떎.

## 12. ?꾩옱 ?먮떒
- IOCP accept path???댁젣 `accept()` thread 湲곕컲???꾨땲??`AcceptEx + IOCP completion` 湲곕컲?대떎.
- accept context slot pool? ?곸슜?섏뼱 ?덈떎.
- `accepted socket reuse`???꾩쭅 湲곕낯 梨꾪깮?섏? ?딆븯怨? ?꾩옱??留?repost留덈떎 ??socket??留뚮뱺??
- ?꾩옱 ?먮떒?쇰줈??socket ?ъ궗?⑹? 湲곕낯媛믪쑝濡?媛뺤젣?섍린蹂대떎 蹂꾨룄 ?ㅽ뿕 ?듭뀡 ?섏??????곸젅?섎떎.



