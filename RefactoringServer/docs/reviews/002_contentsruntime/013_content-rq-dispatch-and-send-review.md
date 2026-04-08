# Content Rq 泥섎━? Send ?먮쫫 由щ럭

## 1. 紐⑹쟻
- ??臾몄꽌??`Content Rq` ?⑦궥???쒕쾭???ㅼ뼱?붿쓣 ???대뼡 寃쎈줈濡?泥섎━?섎뒗吏 ?뺣━?쒕떎.
- ?뱁엳 `EnqueuePacket`, `TryBeginSend`, `EndSend`媛 媛곴컖 ?대뒓 ?④퀎?먯꽌 ?곗씠?붿? `EchoRq -> EchoRp`瑜?湲곗??쇰줈 ?ㅻ챸?쒕떎.
- ?듭떖? `receive 履?肄섑뀗痢???? `send 履??ㅽ듃?뚰겕 ??媛 ?쒕줈 ?ㅻⅨ ?뚯씠?꾨씪?몄씠?쇰뒗 ?먯쓣 遺꾨━?댁꽌 ?댄빐?섎뒗 寃껋씠??

## 2. ??以??붿빟
- `EnqueuePacket`? **?대씪?댁뼵?멸? 蹂대궦 Rq瑜?肄섑뀗痢??ㅻ젅?쒕줈 ?섍린??receive-side ??*??
- `TryBeginSend / EndSend`??**?쒕쾭媛 留뚮뱺 Rp瑜??ㅼ젣 ?뚯폆?쇰줈 ?대낫?대뒗 send-side ?쒖뼱**??
- 利?`EchoRq`??`NetworkLib -> ApplicationHandler -> ContentsRuntime -> ContentThread`濡??ㅼ뼱?ㅺ퀬,
  `EchoRp`??`ContentThread -> ContentsRuntime Bridge -> NetworkLib SendQueue -> WSASend`濡??섍컙??

## 3. Receive 履??먮쫫
### 3.1 IOCP worker媛 content packet??爰쇰궦??- `FIocpServer` worker??`WSARecv` completion ?댄썑 recv buffer瑜??뚯떛?댁꽌 content packet view瑜?留뚮뱺??
- ?뚯떛??packet? `IApplicationHandler::OnPacketReceived(...)`濡??꾨떖?쒕떎.

???肄붾뱶:
- [IApplicationHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\IApplicationHandler.h)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)

以묒슂????
- `packetView`??borrowed view??
- ?좏슚 踰붿쐞??肄쒕갚 ?덉そ肉먯씠??
- 洹몃옒??肄섑뀗痢??ㅻ젅?쒕줈 ?섍만 ?뚮뒗 owned payload濡?諛붽퓭???쒕떎.

### 3.2 EchoServer ???몃뱾?ш? `EnqueuePacket`???몄텧?쒕떎
- `FEchoApplication::OnPacketReceived(...)`?먯꽌 packet opcode瑜?蹂닿퀬 trace瑜??④릿 ??  `m_contentRuntime.EnqueuePacket(sessionId, packetView.opcode, packetView.payload, packetView.payloadLength)`瑜??몄텧?쒕떎.

???肄붾뱶:
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)

???④퀎?먯꽌??`EnqueuePacket` ?섎?:
- ?꾩쭅 send? 愿怨꾩뾾??
- ?꾩옱 ?몄뀡???대뒓 肄섑뀗痢??몄뒪?댁뒪??遺숈뼱 ?덈뒗吏 蹂닿퀬,
  洹?肄섑뀗痢??ㅻ젅?쒖쓽 work queue??`FOwnedPacketEnvelope`瑜??ｋ뒗 ?④퀎??

### 3.3 `ContentsRuntime`媛 ?꾩옱 ?쇱슦?낆쓣 蹂닿퀬 ???肄섑뀗痢??ㅻ젅?쒕줈 ?섍릿??- `FContentRuntime::EnqueuePacket(...)`?
  1. `sessionId -> contentInstanceId -> ownerThread` ?쇱슦?낆쓣 議고쉶?섍퀬
  2. borrowed payload瑜?`std::vector<char>`濡?蹂듭궗??`FOwnedPacketEnvelope`瑜?留뚮뱾怨?  3. `targetThread->EnqueuePacket(...)`???몄텧?쒕떎.

???肄붾뱶:
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)

???④퀎???섎?:
- `NetworkLib` worker thread?먯꽌 吏곸젒 寃뚯엫 濡쒖쭅???ㅽ뻾?섏? ?딅뒗??
- ?ㅽ듃?뚰겕 ?ㅻ젅?쒕뒗 肄섑뀗痢??ㅻ젅??inbox源뚯? ?섍린????븷留??쒕떎.

### 3.4 肄섑뀗痢??ㅻ젅?쒓? ?ㅼ젣 `Rq`瑜?泥섎━?쒕떎
- `FContentThread::EnqueuePacket(...)`? lock-free work queue??`SQueuedWorkItem`???ｊ퀬 worker瑜?源⑥슫??
- worker thread??dequeue ??`content->OnPacket(...)`???몄텧?쒕떎.

???肄붾뱶:
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.cpp)

利?`EchoRq`??寃곌뎅 `FEchoContent::OnPacket(...)`源뚯? ???泥섎━?쒕떎.

## 4. `EchoRq -> EchoRp` 泥섎━ ?먮쫫
### 4.1 `FEchoContent::OnPacket(...)`
- ?꾩옱 room content ?몄뒪?댁뒪? route generation??寃利앺븳 ??opcode瑜?蹂닿퀬 遺꾧린?쒕떎.
- `Generated::Echo::FEchoRq::kOpcode`硫?`HandleEchoRq(...)`濡??ㅼ뼱媛꾨떎.

???肄붾뱶:
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp)

### 4.2 `HandleEchoRq(...)`
- payload瑜?`Generated::Echo::FEchoRq`濡?deserialize?쒕떎.
- 洹??ㅼ쓬 `Generated::Echo::FEchoRp responsePacket;`瑜?留뚮뱾怨?  `ContentsRuntime::Bridge::SendContentPacket(bridge, sessionId, responsePacket)`瑜??몄텧?쒕떎.

?ш린??以묒슂????
- 肄섑뀗痢??ㅻ젅?쒕뒗 `WSASend`瑜?吏곸젒 ?몄텧?섏? ?딅뒗??
- bridge瑜??듯빐 `NetworkLib` 履?send path濡??붿껌???섍릿??

???肄붾뱶:
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp)

## 5. Send 履??먮쫫
### 5.1 `SendContentPacket(...)`
- `SendContentPacket(...)`? packet??`FPacketWriter`濡?serialize?섍퀬
  `bridge.SendRaw(sessionId, opcode, buffer, length)`瑜??몄텧?쒕떎.

???肄붾뱶:
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Bridge\IContentBridge.h)

### 5.2 `FContentRuntime::SendRaw(...)`
- ?꾩옱 `ContentsRuntime`??`IContentBridge` 援ы쁽泥대떎.
- `SendRaw(...)`???대??먯꽌 `NetworkLib::IServer*`瑜?爰쇰궡 `server->Send(...)`瑜??몄텧?쒕떎.

???肄붾뱶:
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)

利?肄섑뀗痢??ㅻ젅?쒓? 留뚮뱺 `EchoRp`???ш린?쒕???`NetworkLib` send path濡??대젮媛꾨떎.

### 5.3 `FIocpServer::Send(...)`
- `FIocpServer::Send(...)`??session???↔퀬
  1. content header瑜?遺숈씠怨?  2. ?꾩슂?섎㈃ cipher/framer瑜??곸슜?섍퀬
  3. `FSendBuffer`瑜?留뚮뱺 ??  4. `sessionContext->EnqueueSendBuffer(...)`
  5. `PostSend(*sessionContext)`
  ?쒖꽌濡?吏꾪뻾?쒕떎.

???肄붾뱶:
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)

?ш린?쒖쓽 `EnqueueSendBuffer`??
- ?ㅽ듃?뚰겕 send queue??
- ?욎뿉??留먰븳 `ContentsRuntime::EnqueuePacket`怨??대쫫??鍮꾩듂?섏?留??꾩쟾???ㅻⅨ ??븷?대떎.

## 6. `TryBeginSend / EndSend`媛 ?섎뒗 ??### 6.1 `EnqueueSendBuffer(...)`
- send buffer瑜?lock-free send queue???ｋ뒗??
- ?숈떆??`kSendPendingFlag`瑜??몄슫??

???肄붾뱶:
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.cpp)

?섎?:
- ?쒖씠 ?몄뀡?먮뒗 ?꾩쭅 蹂대궡????踰꾪띁媛 ?덈떎?앸뒗 ?ъ떎???④릿??

### 6.2 `TryBeginSend()`
- `PostSend()`媛 send瑜??쒖옉?????덈뒗吏 ?먮떒?????대떎.
- ?대? `kSendInFlightFlag`媛 耳쒖졇 ?덉쑝硫?`false`
- ?꾨땲硫?  - `kSendInFlightFlag`瑜?耳쒓퀬
  - `kSendPendingFlag`瑜?吏?곌퀬
  - `true`瑜?諛섑솚?쒕떎.

???肄붾뱶:
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.cpp)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)

?섎?:
- ???몄뀡??????숈떆???щ윭 `WSASend`媛 寃뱀튂吏 ?딄쾶 ?쒕떎.
- 洹몃━怨??쒖씠踰?send ?쒖옉 ?쒖젏源뚯? ?ㅼ뼱??pending?앹쓣 ?닿? 媛?멸컮?ㅺ퀬 ?쒖떆?쒕떎.

### 6.3 `FillSendBatch(...)`
- `TryBeginSend()`媛 ?깃났?섎㈃ send queue?먯꽌 ?щ윭 `FSendBuffer`瑜?爰쇰궡
  active send buffer 紐⑸줉怨?`WSABUF` 諛곗뿴??留뚮뱺??
- 洹??ㅼ쓬 `WSASend(...)`瑜?嫄대떎.

???肄붾뱶:
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.cpp)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)

### 6.4 `EndSend()`
- send completion???붿쓣 ???몄텧?쒕떎.
- `kSendInFlightFlag`瑜??대━怨?
- send ?꾩쨷 ??enqueue媛 ?덉뿀?붿?(`kSendPendingFlag`)瑜?bool濡?諛섑솚?쒕떎.

???肄붾뱶:
- [FIocpSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.cpp)

?섎?:
- ?쒖씠踰?send???앸궗?ㅲ?- ?쒓렇 ?ъ씠 ??踰꾪띁媛 ?ㅼ뼱?붿쑝??send瑜??ㅼ떆 ?쒖옉?댁빞 ?쒕떎??瑜??숈떆???뚮젮二쇰뒗 ?⑥닔??

### 6.5 completion ?댄썑 ?ш린??- `FIocpServer`??send completion ?댄썑
  - `EndSend()`媛 `true`?닿굅??  - queue媛 ?꾩쭅 ?⑥븘 ?덉쑝硫?  ?ㅼ떆 `PostSend()`瑜??몄텧?쒕떎.

???肄붾뱶:
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)

?닿쾶 ?꾩옱 蹂듭썝???덇굅??send 蹂댁옣???듭떖?대떎.

## 7. ?⑥닔 ?몄텧 ?쒖꽌 ?붿빟
`EchoRq -> EchoRp`瑜?媛??吏㏐쾶 ?곕㈃ ?ㅼ쓬 ?쒖꽌??

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
19. ?꾩슂 ??`PostSend(...)` ?ы샇異?
## 8. 寃곕줎
- `EnqueuePacket`? `Rq 泥섎━ ?쒖옉???대떎.
- `TryBeginSend / EndSend`??`Rp ?≪떊 吏곷젹?붿? ?ш린??蹂댁옣`???대떦?쒕떎.
- ?곕씪??`EchoRq`瑜?諛쏆븯?????쒕쾭 ?숈옉?
  - 癒쇱? 肄섑뀗痢??고????먮줈 ?섍린怨?  - 肄섑뀗痢??ㅻ젅?쒖뿉??寃뚯엫 濡쒖쭅???ㅽ뻾?섍퀬
  - 洹?寃곌낵 `EchoRp`瑜??ㅽ듃?뚰겕 send queue???ｌ뼱
  - `TryBeginSend / EndSend`濡??덉쟾?섍쾶 `WSASend`瑜??댁뼱媛??援ъ“??



