# NetworkLib 媛쒖슂

## 1. ??븷
- `NetworkLib`??寃뚯엫 濡쒖쭅??吏곸젒 ?ㅽ뻾?섎뒗 怨꾩링???꾨땲??
- 鍮꾨룞湲??ㅽ듃?뚰겕 I/O, ?몄뀡 ?앸챸二쇨린, ?⑦궥 ?꾨젅?대컢/吏곷젹?? ?좏뵆由ъ??댁뀡 ?⑦궥 ?꾨떖 寃쎄퀎瑜??쒓났?쒕떎.
- ?곸쐞 怨꾩링? `sessionId + packet` ?⑥쐞濡쒕쭔 ?ㅽ듃?뚰겕瑜??ㅻ（怨? 肄섑뀗痢??ㅽ뻾 紐⑤뜽? `ContentsRuntime`媛 留〓뒗??

## 2. ?꾩옱 梨낆엫
- socket ?앹꽦, accept, recv/send, completion 泥섎━
- ?몄뀡 ?앹꽦/?댁젣? ?몄뀡 ?곹깭 愿由?- transport header / content header / packet framing
- packet reader/writer, generated packet 吏곷젹??吏??- borrowed packet view? zero-copy 蹂댁“ 援ъ“
- RTT 怨꾩륫, page pool 媛숈? ?깅뒫 蹂댁“ 湲곕뒫

## 3. ?꾩옱 鍮꾩콉??- 肄섑뀗痢??ㅻ젅???ㅽ뻾 猷⑦봽
- 肄섑뀗痢??꾩씠 洹쒖튃
- 濡쒕퉬, 猷? 留ㅼ튂 媛숈? 寃뚯엫 洹쒖튃
- DB, Redis, ?댁쁺 紐낅졊 怨꾩링

## 4. ?붾젆?곕━ 援ъ“
- `Containers`
  - lock-free queue/stack
- `Crypto`
  - packet cipher ?명꽣?섏씠?ㅼ? 湲곕낯 援ы쁽
- `Memory`
  - lock-free memory pool, TLS memory pool
- `Packet/Buffer`
  - [FPacketBuffer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Buffer\FPacketBuffer.h)
  - [FRecvBuffer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Buffer\FRecvBuffer.h)
  - [FSendBuffer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Buffer\FSendBuffer.h)
- `Packet/Framing`
  - [PacketTypes](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Framing\PacketTypes.h)
  - [ContentHeader](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Framing\ContentHeader.h)
  - [IPacketFramer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Framing\IPacketFramer.h)
  - [FDefaultPacketFramer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Framing\FDefaultPacketFramer.h)
- `Packet/Serialization`
  - [FPacketReader](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketReader.h)
  - [FPacketWriter](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketWriter.h)
  - [FPacketSerialization](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketSerialization.h)
  - [IContentPacket](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\IContentPacket.h)
- `Packet/View`
  - [FPacketView](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\View\FPacketView.h)
  - [FBorrowedViewGuard](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\View\FBorrowedViewGuard.h)
- `Servers/Core`
  - [BackendTypes](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\BackendTypes.h)
  - [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FServerFactory.h)
  - [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
  - [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)
  - [FStubServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FStubServer.h)
- `Servers/Session`
  - [ISession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\ISession.h)
  - [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.h)
  - [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)
- `Servers`
  - [IServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\IServer.h)
  - [IApplicationHandler](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\IApplicationHandler.h)

## 5. Backend 媛쒖슂
- public 寃쎄퀎??[IServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\IServer.h) ?섎굹濡??좎??쒕떎.
- backend ?좏깮? [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FServerFactory.h)媛 留〓뒗??
- ?꾩옱 backend ?곹깭
  - `Iocp` -> [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
  - `Rio` -> [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)
  - `BoostAsio` -> [FStubServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FStubServer.h)

## 6. IOCP ?꾩옱 ?곹깭
- `FIocpServer`???꾩옱 `accept()` thread 湲곕컲???꾨땲??`AcceptEx + IOCP completion` 湲곕컲?대떎.
- accept??`SAcceptContext[]` 怨좎젙 諛곗뿴???ъ슜???щ윭 slot??誘몃━ pre-post ?쒕떎.
- accept completion? worker??`GetQueuedCompletionStatus()` 猷⑦봽?먯꽌 recv/send completion怨??④퍡 泥섎━?쒕떎.
- ?꾩옱??`accept context slot pool`? ?곸슜?섏뼱 ?덉?留? `accepted socket reuse`??湲곕낯 梨꾪깮?섏? ?딆븯??

## 7. RIO ?꾩옱 ?곹깭
- `FRioServer`??pure `RIO` baseline??援ы쁽?섏뼱 ?덈떎.
- ?꾩옱 鍮꾧탳 寃곌낵 湲곗??쇰줈 `Rio`??湲곕낯 send ?뺤콉? `Direct` ?좎?媛 ?곸젅?섎떎.
- `OwnerThread` send 寃쎈줈???꾩냽 理쒖쟻???ㅽ뿕 寃쎈줈濡??④꺼?붾떎.

## 8. Session 媛쒖슂
- ?몄뀡? backend蹂?援ы쁽?쇰줈 ?섎돏??
- 怨듯넻 寃쎄퀎??[ISession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\ISession.h)?대떎.
- `IOCP` 寃쎈줈??[FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.h)
- `RIO` 寃쎈줈??[FRioSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)

## 9. ?곸쐞 怨꾩링怨쇱쓽 寃쎄퀎
- `NetworkLib`??`sessionId + packet` ?꾨떖源뚯?留?梨낆엫吏꾨떎.
- 肄섑뀗痢??ㅻ젅?? 肄섑뀗痢??꾩씠, 肄섑뀗痢좊퀎 ?ㅽ뻾 紐⑤뜽? `ContentsRuntime`媛 留〓뒗??
- ?ㅽ듃?뚰겕 怨꾩링怨?寃뚯엫 濡쒖쭅 ?ㅽ뻾 紐⑤뜽??媛뺥븯寃?寃고빀?섏? ?딅뒗 寃껋씠 ?꾩옱 援ъ“??紐⑺몴??

## 10. ?ㅻ뜑 / PCH 洹쒖튃
- 怨듭슜 ?명꽣?섏씠???ㅻ뜑??forward declaration???곗꽑 ?ъ슜?쒕떎.
- ?먯＜ 諛붾뚯? ?딅뒗 怨듭슜 ?섏〈?깆? [NetLibPch.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\NetLibPch.h)濡?紐⑥???
- ?먯꽭??洹쒖튃? [003_cpp-header-pch-convention.md](D:\Project\ServerPortfolio\RefactoringServer\docs\architecture\001_project\003_cpp-header-pch-convention.md)瑜??곕Ⅸ??

