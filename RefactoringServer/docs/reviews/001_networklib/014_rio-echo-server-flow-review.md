# RIO EchoServer Flow Review

## 1. 紐⑹쟻
- ?꾩옱 湲곗? `Backend: Rio`?먯꽌 `EchoClient -> EchoServer -> ContentsRuntime -> EchoServer -> EchoClient` ?먮쫫???ㅼ젣濡??대뼸寃??댁뼱吏?붿? 肄붾뱶 ?몄텧 ?ㅽ깮 湲곗??쇰줈 ?뺣━?쒕떎.
- ?뱁엳 ?ㅼ쓬 吏덈Ц??諛붾줈 ?듯븷 ???덇쾶 ?섎뒗 寃껋씠 紐⑹쟻?대떎.
  - `EchoClient`媛 `Rq`瑜?留뚮뱾怨?蹂대궡??寃쎈줈???대뵒?멸?
  - `EchoServer`??`AcceptEx` 湲곕컲 accept ?먮쫫? ?대뼸寃??댁뼱吏?붽?
  - `RIO` recv completion???대뼸寃?`ContentsRuntime`源뚯? ?щ씪媛?붽?
  - `EchoRp`媛 ?대뼡 寃쎈줈濡?`RIOSend()`源뚯? ?대젮媛?붽?

## 2. ????뚯씪
- ?쒕쾭 ?쒖옉 / application 寃쎄퀎
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
- RIO backend
  - [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)
- RIO session
  - [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)
  - [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.cpp)
- 肄섑뀗痢??고???  - [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.h)
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)
  - [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Bridge\IContentBridge.h)
- ?⑦궥 吏곷젹??/ send packet 寃쎄퀎
  - [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketSerialization.h)
  - [FPacketWriter.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketWriter.h)
- Echo content
  - [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp)
- ?대씪?댁뼵??  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoClient\Main.cpp)

## 3. ?꾩옱 援ъ“ ?붿빟
- `EchoClient`???ъ쟾??raw Winsock?쇰줈 ?숈옉?섏?留? ?⑦궥 吏곷젹???꾨젅?대컢? `NetworkLib`瑜??ъ슜?쒕떎.
- `EchoServer`??`IApplicationHandler` 援ы쁽泥댁씤 `FEchoApplication`???듯빐 transport? `ContentsRuntime`瑜??곌껐?쒕떎.
- `RIO` backend??`AcceptEx + WSA_FLAG_REGISTERED_IO + RIO_EVENT_COMPLETION` 議고빀?쇰줈 ?숈옉?쒕떎.
- ?꾩옱 send 寃쎄퀎???덉쟾 `SendRaw(opcode, buffer, length)`媛 ?꾨땲??`SendPacket(sessionId, FOutgoingContentPacket&&)` 湲곗??대떎.
- ??蹂寃쎌쑝濡??좏뵆由ъ??댁뀡 怨꾩링? `NetworkLib` transport header瑜?吏곸젒 ?ㅻ（吏 ?딅뒗??

## 4. EchoClient?먯꽌 Rq瑜?蹂대궡???먮쫫
### 4-1. ?몄뀡 猷⑦봽 吏꾩엯
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoClient\Main.cpp)
  - `main()`
  - `RunSingleSession(sessionIndex, options, rttMetricsRuntime)`

### 4-2. ?곌껐
- `RunSingleSession(...)`
  - `TryConnectSocket(options, clientSocket, errorMessage)`
- `TryConnectSocket(...)`
  - `socket()`
  - `connect()`
  - ?꾩슂 ??`SO_RCVTIMEO` ?ㅼ젙

### 4-3. 濡쒓렇??/ 猷?吏꾩엯 bootstrap
- 媛숈? `RunSingleSession(...)` ?덉뿉???쒖꽌?濡?吏꾪뻾?쒕떎.
  - `FLoginRq` ?꾩넚
  - `FRoomListRq` ?섏떊
  - `FRoomEnterRq` ?꾩넚
  - `FRoomEnterRp` ?섏떊
- ?뺤긽 bootstrap ?꾩뿉留?`EchoRq` 猷⑦봽濡??ㅼ뼱媛꾨떎.

### 4-4. EchoRq 吏곷젹?붿? ?꾩넚
- `RunSingleSession(...)`
  - `Generated::Echo::FEchoRq requestPacket`
  - `NetworkLib::Packet::Serialization::SerializeContentPacket(requestPacket)`
  - `packetCipher.Encode(...)`
  - `packetFramer.BuildPacket(...)`
  - `SendPacketWithOptionalChunking(...)`

?꾩옱 `SerializeContentPacket(...)` ?대???
- [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketSerialization.h)
  - `BuildOutgoingContentPacket(packet).MoveBuffer()`

利??대씪?댁뼵?몃룄 ?꾩옱??
- writer媛 ?욎そ??`SContentHeader` 怨듦컙???↔퀬
- body瑜??ㅼ뿉 serialize????- `NetworkLib` 履?helper媛 content header瑜?梨꾩썙???꾩꽦??content payload瑜?留뚮뱺??

## 5. EchoServer ?쒖옉 ?먮쫫
### 5-1. config? backend ?좏깮
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
  - `FEchoServerConfigLoader::LoadFromFile(...)`
  - `ApplyEchoServerConfigDocument(...)`
  - `FServerFactory::Create(serverConfig.backendKind)`

`Backend: Rio`?대㈃:
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FServerFactory.cpp)
  - `FRioServer` ?앹꽦

### 5-2. Start ?몄텧
- `server->Start(serverConfig, echoApplication)`
- `echoApplication`? `IApplicationHandler` 援ы쁽泥대떎.

## 6. RIO accept ?몄텧 ?ㅽ깮
### 6-1. Start ?대? 珥덇린??- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)
  - `FRioServer::Start(...)`

二쇱슂 ?쒖꽌:
1. `InitializeWinsock()`
2. `LoadRioFunctionTable()`
3. `OpenListenSocket()`
4. `LoadAcceptExFunction()`
5. `StartWorkers()`
6. `m_acceptThread = std::thread(&FRioServer::AcceptLoop, this)`
7. `m_applicationHandler->OnServerStarted(*this)`

### 6-2. AcceptEx 猷⑦봽
- `FRioServer::AcceptLoop()`

?ㅼ젣 ?먮쫫:
1. `WSASocketW(..., WSA_FLAG_REGISTERED_IO)`濡?accepted socket ?꾨낫 ?앹꽦
2. `AcceptEx(...)` ?몄텧
3. event wait
4. ?꾨즺 ??`SO_UPDATE_ACCEPT_CONTEXT`
5. `AttachAcceptedSocket(clientSocket)`

利?RIO 寃쎈줈??accept??
- `accept()`媛 ?꾨땲??`AcceptEx`
- accepted socket??誘몃━ `WSA_FLAG_REGISTERED_IO`濡?留뚮뱺??

### 6-3. AttachAcceptedSocket ?몄텧 ?ㅽ깮
- `FRioServer::AttachAcceptedSocket(clientSocket)`

二쇱슂 ?쒖꽌:
1. `ChooseLeastLoadedWorkerIndex()`
2. 鍮?session slot ?먯깋
3. `FRioSession::Create()`
4. `FRioSession::Initialize(...)`
5. recv staging buffer??`RIORegisterBuffer(...)`
6. `RIOCreateRequestQueue(...)`
7. slot attach
8. `m_applicationHandler->OnClientConnected(sessionId)`
9. `PostRecv(*newSessionContext)`

?꾩옱 owner worker ?뺤콉?:
- `activeSessionCount` 湲곕컲 least-loaded
- accept ??owner瑜??뺥븯怨??몄뀡? 洹?worker??怨좎젙?쒕떎.

## 7. RIO recv -> EchoServer application -> ContentsRuntime ?먮쫫
### 7-1. 泥?recv post
- `FRioServer::PostRecv(FRioSession&)`
  - recv pending ?곹깭 泥댄겕
  - staging buffer瑜?`RIO_BUF`濡??ㅼ젙
  - `RIOReceive(...)`

### 7-2. CQ ?뚮퉬
- `FRioServer::WorkerLoop(workerIndex)`

二쇱슂 ?쒖꽌:
1. `DrainSendCommands(workerIndex)`
2. `RIONotify(worker.completionQueue)`
3. `WaitForSingleObject(worker.completionEvent, ...)`
4. `RIODequeueCompletion(...)`
5. 媛?completion留덈떎 `HandleRioCompletion(...)`

### 7-3. recv completion 泥섎━
- `FRioServer::HandleRioCompletion(...)`
  - `requestKind == Recv`硫?`HandleRecvCompletion(...)`

- `FRioServer::HandleRecvCompletion(...)`
  - staging buffer -> session recv ring buffer 蹂듭궗
  - `m_packetFramer->TryExtractPacketView(...)`
  - checksum 寃利?  - ?꾩슂 ??`packetCipher->Decode(...)`
  - `TryParseContentPacketView(...)`
  - `m_applicationHandler->OnPacketReceived(*this, sessionId, contentPacketView)`

?ш린源뚯?媛 transport 怨꾩링 梨낆엫?대떎.

## 8. EchoServer application dispatch ?먮쫫
### 8-1. ?곌껐 吏곹썑
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
  - `FEchoApplication::OnClientConnected(sessionId)`
  - `m_contentRuntime.EnterSession(sessionId, kAuthContentId)`

利????몄뀡? 癒쇱? `Auth` content濡??ㅼ뼱媛꾨떎.

### 8-2. packet ?섏떊 ??- `FEchoApplication::OnPacketReceived(server, sessionId, packetView)`
  - ?꾩슂 ??trace/log
  - `m_contentRuntime.EnqueuePacket(sessionId, packetView.opcode, packetView.payload, packetView.payloadLength)`

利?`FRioServer`??contents thread瑜?吏곸젒 ?몄텧?섏? ?딄퀬:
- `IApplicationHandler`
- `ContentsRuntime`
?쒖꽌濡??섍릿??

## 9. ContentsRuntime -> Echo content ?먮쫫
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `FContentRuntime::EnqueuePacket(...)`
  - route lookup
  - target `FContentThread`??envelope enqueue

- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp)
  - `FEchoContent::OnPacket(...)`
  - opcode switch
  - `HandleEchoRq(...)`
  - `HandleRoomListRq(...)`
  - `HandleRoomChangeRq(...)`

利?`EchoRq`??理쒖쥌?곸쑝濡?
1. `FRioServer`
2. `FEchoApplication`
3. `FContentRuntime`
4. `FContentThread`
5. `FEchoContent::HandleEchoRq`
?쒖꽌濡??ㅼ뼱媛꾨떎.

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

### 10-3. RIO send 怨꾩링
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)
  - `FRioServer::SendPacket(...)`

?꾩옱 湲곗? 二쇱슂 ?쒖꽌:
1. `AcquireSession(sessionId)`
2. `packet.ReleaseBuffer()`
3. framer媛 ?덉쑝硫?`BuildPacket(...)`?쇰줈 transport packet ?앹꽦
4. ?꾩옱 mode媛 `Direct`硫?`SubmitSendDirect(...)`
5. `SubmitSendDirect(...)`
   - `RIORegisterBuffer(...)`
   - `RIOSend(...)`

利??꾩옱 湲곕낯 `Rio Direct` ?먮쫫?먯꽌??
- contents thread媛 留뚮뱺 outgoing packet
- `FRioServer::SendPacket`
- `SubmitSendDirect`
- `RIOSend`
?쒖꽌濡?媛꾨떎.

## 11. EchoClient?먯꽌 Rp瑜?諛쏅뒗 ?먮쫫
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoClient\Main.cpp)
  - `RunSingleSession(...)`
  - `tryReceiveNextContentPacket(...)`

二쇱슂 ?쒖꽌:
1. `recv()` ?먮뒗 timeout 湲곕컲 wait
2. `packetFramer.TryExtractPacketView(...)`
3. checksum 寃利?4. `packetCipher.Decode(...)`
5. `TryParseContentPacketView(...)`
6. `DeserializeContentPacket(contentPacketView, responsePacket)`

利?EchoClient 履??묐떟 ?섏떊?:
- transport header ?쒓굅
- content header ?쒓굅
- generated packet deserialize
?쒖꽌??

## 12. ?꾩옱 湲곗??먯꽌 以묒슂?섍쾶 諛붾???1. ?덉쟾 `SendRaw(opcode, buffer, length)` 寃쎈줈媛 ?꾨땲??`SendPacket(FOutgoingContentPacket&&)` 寃쎈줈??
2. `SContentHeader`????怨꾩링??吏곸젒 ?곗? ?딅뒗??
3. `BuildOutgoingContentPacket(...)`媛 front headroom???댁슜??content payload瑜??꾩꽦?쒕떎.
4. ??蹂寃쎌쑝濡?`SContentHeader + body` ?щ났?ш? 以꾩뿀??

## 13. ?꾩옱 ?댁꽍
- `RIO` backend???댁젣 stub???꾨땲?? `EchoServer`?먯꽌 accept/recv/send/contents dispatch ?꾩껜 ?먮쫫???ㅼ젣濡??곌껐???곹깭??
- `EchoClient`???end-to-end ?먮쫫???꾩옱 `SendPacket` 寃쎈줈 湲곗??쇰줈 ?뺣━ 媛?ν븯??
- ?댄썑 ?깅뒫 理쒖쟻?붾뒗 ??援ъ“ ?꾩뿉??
  - broadcast fan-out
  - registered buffer 鍮꾩슜 理쒖쟻??  - owner-thread send 異붽? ?ㅽ뿕
?쒖꽌濡?蹂대뒗 寃?留욌떎.



