# RIO Direct Cross-Thread SendRing Cache Review

## 1. 紐⑹쟻
- `EchoServer` 湲곗??먯꽌 `RIO Direct`媛 `RIO OwnerThread`蹂대떎 遺덈━?섍쾶 ?섏삩 ?먯씤???뺣━?쒕떎.
- ?대쾲 臾몄꽌???듭떖 媛?ㅼ? `Direct`??二쇱썝?몄씠 ?⑥닚 `sendRingMutex` ?湲곕낫?? `content worker`? `RIO owner worker`媛 媛숈? session send ring ?곹깭瑜?踰덇컝??留뚯?硫댁꽌 諛쒖깮?섎뒗 cache 臾댄슚?붿? cache line ping-pong?대씪??寃껋씠??
- ??臾몄꽌???뺤젙 寃곕줎???꾨땲???꾩옱 肄붾뱶? 痢≪젙 寃곌낵瑜?諛뷀깢?쇰줈 ??`?곗꽑 媛???뺣━ 臾몄꽌`??

## 2. 諛곌꼍
- 愿??寃곌낵 ?뺣━:
  - [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)
- 愿??援ы쁽 臾몄꽌:
  - [014_rio-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\014_rio-echo-server-flow-review.md)
  - [015_rio-send-dispatch-mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\015_rio-send-dispatch-mode-review.md)
  - [019_rio-session-send-ring-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\019_rio-session-send-ring-review.md)

?대쾲??臾몄젣瑜?蹂?議곌굔:
- `EchoServer`
- `PayloadSize = 16`
- `SessionCount = 250`
- `HoldSeconds = 7200`
- `IntervalMs = 0`
- `WorkerThreadCount = 4`
- ?쒕쾭? ?대씪?댁뼵?몃? 媛숈? 癒몄떊?먯꽌 ?숈떆 ?ㅽ뻾

## 3. 愿李?- `Windows Server 4肄붿뼱`??`EchoServer 2?쒓컙 4紐⑤뱶` 寃곌낵?먯꽌??`RIO OwnerThread`媛 `RIO Direct`蹂대떎 泥섎━?됱씠 ?ш쾶 ?믨퀬 CPU? ?됯퇏 RTT????醫뗭븯??
- 諛섎㈃ `EchoServer` ?ㅼ젙 ?먯껜??`sendThreadCount = 1`, `responsesPerThread = 1`?대씪?? 媛숈? ?몄뀡??????щ윭 content thread媛 ?숈떆???묐떟???섎뒗 媛뺥븳 producer 寃쎌웳? ?ъ? ?딅떎.

洹쇨굅:
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp#L293)
- [022_echo-server-windowsserver-2h-4mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\022_echo-server-windowsserver-2h-4mode-review.md)

利??대쾲 耳?댁뒪??"`?숈떆 producer媛 留롮븘??Direct lock 寃쏀빀???ы뻽??"濡쒕쭔 ?ㅻ챸?섍린 ?대졄??

## 4. ?꾩옱 肄붾뱶 寃쎈줈
### 4-1. Echo ?묐떟? content worker?먯꽌 諛붾줈 `server->SendPacket()`?쇰줈 ?대젮媛꾨떎
- `FEchoContent`??湲곕낯 ?ㅼ젙?먯꽌 echo ?붿껌 1媛쒕떦 ?묐떟 1媛쒕? 諛붾줈 蹂대궦??
  - [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp#L293)
- `ContentsRuntime`?????묐떟??怨㏓컮濡?transport??`SendPacket()`?쇰줈 ?꾨떖?쒕떎.
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp#L654)

利?`Direct` 紐⑤뱶??send submit 吏꾩엯 ?ㅻ젅?쒕뒗 contents worker 履쎌씠??

### 4-2. `Direct`??content worker媛 session send ring??吏곸젒 留뚯쭊??- `FRioServer::SendPacket()`?먯꽌 `Direct`硫?諛붾줈 `AppendPacketToSendRing(..., true)` ??`PostSend()`瑜??섑뻾?쒕떎.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L232)
- ?대븣 `AppendPacketToSendRing()`? `sendRingMutex` ?덉뿉??  - cipher encode
  - checksum 怨꾩궛
  - framing
  - send ring append
瑜??섑뻾?쒕떎.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L868)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L931)
- ?댁뼱??`PostSend()`??`Direct`硫??ㅼ떆 `sendRingMutex`瑜??↔퀬 `TryPrepareNextSend()`瑜??몄텧?쒕떎.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L997)

### 4-3. send completion? RIO owner worker媛 泥섎━?쒕떎
- RIO completion? worker loop?먯꽌 泥섎━?쒕떎.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L1349)
- send completion???ㅻ㈃ `HandleSendCompletion()`?먯꽌 `Direct` 紐⑤뱶???ㅼ떆 `sendRingMutex`瑜??↔퀬 `CompleteCurrentSend()`瑜??섑뻾?쒕떎.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L1484)

利?`Direct`??- send submit: contents worker
- send completion: RIO owner worker
媛 媛숈? session send ring ?곹깭瑜?踰덇컝??留뚯쭊??

### 4-4. `OwnerThread`??ring 議곗옉??owner worker濡?紐⑥???- `OwnerThread`??`SendPacket()` ?쒖젏??ring??吏곸젒 留뚯?吏 ?딄퀬, lock-free queue??packet留??곸옱?쒕떎.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L232)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L1066)
- ?댄썑 owner worker媛 send command瑜?drain?섎㈃??ring append? `PostSend()`瑜?泥섎━?쒕떎.
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L763)
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L800)
- ??寃쎈줈?먯꽌??`AppendPacketToSendRing(..., false)`瑜??ъ슜?섎?濡?send ring? ?ㅼ쭏?곸쑝濡??⑥씪 owner thread媛 留뚯쭊??
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L805)

## 5. ?듭떖 媛??### 5-1. `EchoServer`?먯꽌??媛뺥븳 mutex 寃쎌웳蹂대떎 cross-thread ping-pong?????좊젰?섎떎
- ?꾩옱 `EchoServer` 湲곕낯 寃쎈줈?먯꽌?????붿껌?????묐떟?대씪 媛숈? ?몄뀡??????щ윭 content thread媛 ?숈떆??`SendPacket()`???뚮━??援ъ“媛 ?꾨땲??
- ?곕씪??`sendRingMutex`媛 ?щ윭 producer ?ъ씠?먯꽌 ?ㅻ옒 block?섎뒗 ?뺥깭??寃쏀빀? ?대쾲 ?뚰겕濡쒕뱶?먯꽌 二쇱썝?몄씪 媛?μ꽦????떎.
- ?섏?留?`Direct`??contents worker? owner worker媛 媛숈? session send ring ?곹깭瑜?援먮?濡?媛깆떊?쒕떎.

?뱁엳 ?ㅼ쓬 ?곹깭媛 ?먯＜ 諛붾먮떎.
- `m_sendRingReadOffset`
- `m_sendRingWriteOffset`
- `m_sendRingUsedBytes`
- `m_sendRingInFlightBytes`
- `m_sendRequestContext`
- `m_sendRingBuffer`???ㅼ젣 payload ?곸뿭

洹쇨굅:
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h#L145)

??援ъ“硫?- contents worker媛 append/prepare瑜??꾪빐 ring ?곹깭瑜?write
- owner worker媛 completion ??ring ?곹깭瑜?write
- ?ㅼ떆 contents worker媛 ?ㅼ쓬 send瑜??꾪빐 same cache line??write
?섎뒗 ?앹쑝濡?cache line ownership??怨꾩냽 ?대룞?쒕떎.

?닿쾬??loopback echo泥섎읆 ?붿껌/?묐떟 ?뚯쟾??留ㅼ슦 鍮좊Ⅸ ?섍꼍?먯꽌??mutex ?湲곕낫??????鍮꾩슜?쇰줈 蹂댁씪 ???덈떎.

### 5-2. `Direct`???꾧퀎援ъ뿭??湲몄뼱??ping-pong ?곹뼢????而ㅼ쭊??- `sendRingMutex` ?덉뿉??offset 媛깆떊留??섎뒗 寃껋씠 ?꾨땲??encode, checksum, framing, append源뚯? 紐⑤몢 ?섑뻾?쒕떎.
- 洹몃윭硫?lock hold time??湲몄뼱吏怨? 媛숈? cache line??遺숈옟怨??덈뒗 ?쒓컙???섏뼱?쒕떎.
- 寃쎌웳 ?ㅻ젅???섍? 留롮? ?딆븘?? ???ㅻ젅???ъ씠?먯꽌 ownership ?댁쟾 鍮꾩슜??諛섎났?섎㈃ ?꾩쟻 ?먯떎??而ㅼ쭏 ???덈떎.

### 5-3. `OwnerThread`??ring locality瑜??삳뒗 ???handoff 鍮꾩슜???몃떎
- `OwnerThread`??contents worker媛 吏곸젒 ring??留뚯?吏 ?딅뒗??
- ???lock-free queue enqueue? send command enqueue媛 ?ㅼ뼱媛꾨떎.
- ??援ъ“??handoff 鍮꾩슜? ?덉?留? session send ring ?먯껜??owner worker ??怨녹뿉 臾띠씤??

利?`EchoServer` 媛숈? ?묒? payload, 鍮좊Ⅸ turn-around, 媛숈? 癒몄떊 loopback 議곌굔?먯꽌??
- `Direct`??cross-thread ring ping-pong 鍮꾩슜
- `OwnerThread`??handoff 鍮꾩슜
以??꾩옄媛 ??而ㅼ졇??`OwnerThread`媛 ?좊━?덉쓣 媛?μ꽦???믩떎.

## 6. ??ChattingServer? ?ㅻⅨ媛
- `ChattingServer`??broadcast, room routing, contents runtime queue, dummy client event backlog媛 媛숈씠 ?욎씤 end-to-end workload??
- ???섍꼍?먯꽌??owner handoff 鍮꾩슜, owner queue backlog, contents scheduling 李⑥씠媛 ???ш쾶 ?쒕윭?????덈떎.
- 諛섎㈃ ?대쾲 `EchoServer`??嫄곗쓽 `small packet echo hot path`??媛源뚯썙??ring locality 李⑥씠媛 ??吏곸젒?곸쑝濡??쒕윭?쒕떎.

利?
- `EchoServer` 寃곌낵瑜?transport microbenchmark濡?蹂닿퀬
- `ChattingServer` 寃곌낵瑜?contents end-to-end benchmark濡??곕줈 ?댁꽍?댁빞 ?쒕떎.

## 7. ?꾩옱 ?먮떒
- ?꾩옱 肄붾뱶? 寃곌낵瑜??④퍡 蹂대㈃, `EchoServer`?먯꽌 `Direct`媛 `OwnerThread`蹂대떎 ?먮┛ ?댁쑀瑜?`sendRingMutex`??媛뺥븳 ?湲?寃쏀빀 ?섎굹濡??ㅻ챸?섎뒗 寃껋? 遺議깊븯??
- ?ㅽ엳?????ㅻ뱷???덈뒗 媛?ㅼ?:
  - `Direct`?먯꽌 contents worker? owner worker媛 媛숈? session send ring ?곹깭瑜?踰덇컝??議곗옉?섍퀬
  - 洹?怨쇱젙?먯꽌 cache invalidation / cache line ping-pong???꾩쟻?섎ŉ
  - 嫄곌린??湲??꾧퀎援ъ뿭怨???? lock/unlock, per-packet submit ?⑦꽩???⑹퀜議뚮떎??寃껋씠??

## 8. ?뺤씤???꾩슂??怨꾩륫
- `sendRingMutex wait time`
- `sendRingMutex hold time`
- `Direct`?먯꽌 session蹂?send submit ?ㅻ젅??遺꾪룷
- `RIOSend calls/sec`
- `avg bytes per RIOSend`
- send completion ?댄썑 ?ㅼ쓬 `PostSend()`源뚯???吏??- session send ring 愿???꾨뱶??cache locality瑜??뺤씤?????덈뒗 ETW / sampling profiler ?곗씠??
## 9. ?ㅼ쓬 ?≪뀡
- `Direct`?먯꽌 `encode / checksum / framing`??lock 諛뽰쑝濡?鍮쇰뒗 ?ㅽ뿕
- `Append + TryPrepareNextSend`瑜???踰덉쓽 lock 援ш컙?쇰줈 ?⑹튂???ㅽ뿕
- completion path?먯꽌 `CompleteCurrentSend + TryPrepareNextSend`瑜???踰덉쓽 lock 援ш컙?쇰줈 ?⑹튂???ㅽ뿕
- ??蹂寃??꾪썑濡?`EchoServer` 媛숈? same-machine loopback 議곌굔?먯꽌 ?ъ륫??


