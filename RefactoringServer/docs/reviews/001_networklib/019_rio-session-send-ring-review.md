# RIO Session Send Ring Review

## 1. 紐⑹쟻
- `RIO` send 寃쎈줈瑜?`per-packet send buffer` 諛⑹떇?먯꽌 `session-local send ring` 諛⑹떇?쇰줈 諛붽씔 援ы쁽??由щ럭?쒕떎.
- ?대쾲 由щ럭??珥덉젏? correctness, `Direct / OwnerThread` 紐⑤뱶蹂?ownership, 洹몃━怨??꾩옱 ?뚯뒪??踰붿쐞?먯꽌 ?쒕윭???⑥? 由ъ뒪???뺣━??

## 2. 踰붿쐞
寃??????뚯씪:
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)
- [FRioServer.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.cpp)
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)

愿??湲고쉷??
- [014_rio-registered-buffer-pool.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\014_rio-registered-buffer-pool.md)
- [015_rio-send-hot-path-overhead-reduction.md](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\007_networklib-performance\015_rio-send-hot-path-overhead-reduction.md)

## 3. Findings
### 3-1. Blocking finding ?놁쓬
- ?꾩옱 援ы쁽 湲곗??쇰줈??`blocking` 湲?correctness 臾몄젣??李얠? 紐삵뻽??
- `RIO Direct`, `RIO OwnerThread` 紐⑤몢 ??`session send ring` 寃쎈줈濡?`60珥? ?ㅻえ?ъ? `5遺? 怨좊????곗쓣 ?듦낵?덈떎.
- ?뚯뒪??濡쒓렇?먯꽌 ?섎룄??fail-fast 議곌굔??`send stall`, `packet exceeded max send packet size`, `RIOSend failed`???ы쁽?섏? ?딆븯??

### 3-2. 湲곗〈 愿痢≪꽦 怨듬갚? ?꾩냽 ?묒뾽?쇰줈 ?댁냼?먮떎
- 珥덇린 援ы쁽 ?쒖젏?먮뒗 `queuedSendBuffers`媛 ?ъ떎??`OwnerThread` ??湲몄씠留?諛섏쁺?댁꽌, `Direct` 紐⑤뱶???ㅼ젣 ring ?뺣젰????蹂댁씠吏 ?딅뒗 怨듬갚???덉뿀??
- ?댄썑 ?꾩냽 ?묒뾽?쇰줈 ?몄뀡/?쒕쾭 ?듦퀎???ㅼ쓬 媛믪씠 異붽??먮떎.
  - `sendRingUsedBytes`
  - `sendRingInFlightBytes`
  - `maxObservedSendRingUsedBytes`
  - `totalSendRingUsedBytes`
  - `totalSendRingInFlightBytes`
- ?곕씪???꾩옱??`Direct` 紐⑤뱶?먯꽌??`queuedSendBuffers=0`怨?蹂꾧컻濡??ㅼ젣 ring pressure瑜??댁쁺 濡쒓렇?먯꽌 ?댁꽍?????덈떎.

### 3-3. 湲곗〈 Direct ?덉쇅 寃쎈줈 lock 踰붿쐞 ?곕젮???꾩냽 ?묒뾽?쇰줈 ?뺣━?먮떎
- 珥덇린 援ы쁽 ?쒖젏?먮뒗 [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp) ??`AppendPacketToSendRing()`媛 `m_sendRingMutex` ?덉뿉??諛붾줈 `CloseSession()`源뚯? ?몄텧?덈떎.
- ?꾩냽 ?묒뾽?먯꽌 ??寃쎈줈??`lock ?덉뿉???ㅽ뙣 ?먯젙留??섑뻾 -> lock 諛뽰뿉??寃쎄퀬 濡쒓렇 + CloseSession()` 援ъ“濡??뺣━?먮떎.
- ?댄썑 `OversizePacket`怨?`SendStall`??媛뺤젣濡??ы쁽?대룄 ?몄뀡? ?ъ쟾??fail-fast濡??ロ삍怨? lock 踰붿쐞 異뺤냼 ??deadlock?대굹 ?댁쨷 close 吏뺥썑??蹂댁씠吏 ?딆븯??

### 3-4. OwnerThread 寃쎈줈??packet buffer瑜?raw pointer濡??볦쑝誘濡?byte ?⑥쐞 ?뺣젰 吏?쒓? ?덉쑝硫?醫뗫떎
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.cpp) ??`EnqueueOwnerSendPacket()`??`FPacketBuffer*`瑜??먯뿉 ?곸옱?섍퀬 媛쒖닔留?移댁슫?명븳??
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp) ??`DrainOwnerThreadSendQueue()`媛 ?쒕젅?명븯湲??꾧퉴吏??packet buffer媛 洹몃?濡??댁븘 ?덉쑝誘濡? queue count留뚯쑝濡쒕뒗 ?ㅼ젣 硫붾え由??뺣젰??異⑸텇???ㅻ챸?섏? 紐삵븳??

沅뚯옣:
- OwnerThread 紐⑤뱶?먮뒗 `queuedOwnerSendBytes` 媛숈? byte 湲곕컲 吏?쒕? 媛숈씠 ?④린硫??쒕떇???ъ썙吏꾨떎.

## 4. 援ы쁽 ?붿빟
### 4-1. ?몄뀡蹂?send ring
- 媛?`FRioSession`? `64KiB` ?ш린??`m_sendRingBuffer`? `RIO_BUFFERID`瑜?媛吏꾨떎.
- send ring register??per-send媛 ?꾨땲???몄뀡 媛앹껜 ?섎챸??遺숇뒗??
- ?濡??뚯븘媛??뚮뒗 ring ?곹깭留?reset?섍퀬, `RIODeregisterBuffer`???쒕쾭 醫낅즺 ??`ReleaseAllSendRingRegistrations()`?먯꽌 ?쇨큵 泥섎━?쒕떎.

### 4-2. ?⑦궥 ?ш린 / stall ?뺤콉
- 理쒕? packet ?ш린??`8KiB`??
- ?대? ?섎뒗 packet? 鍮꾩젙?곸쑝濡?蹂닿퀬 諛붾줈 ?몄뀡???ル뒗??
- `64KiB` ring??媛??李⑥꽌 ?뺤긽 packet append媛 遺덇??ν빐???뺤긽 backpressure濡??④린吏 ?딄퀬 `send stall`濡?蹂닿퀬 ?몄뀡???ル뒗??

### 4-3. ?몄뀡??in-flight send 1媛?- `FRioSession::TryPrepareNextSend()`??`m_sendRingInFlightBytes == 0`???뚮쭔 ?ㅼ쓬 send瑜?以鍮꾪븳??
- `HandleSendCompletion()`?먯꽌 `CompleteCurrentSend()` ???⑥? ?곗씠?곌? ?덉쑝硫?`PostSend()`濡??ㅼ쓬 send瑜??댁뼱 嫄대떎.
- 利??몄뀡??outstanding send????긽 1媛쒕줈 ?좎??쒕떎.

### 4-4. 紐⑤뱶蹂?ownership
- `Direct`
  - ?몄텧 ?ㅻ젅?쒓? `SendPacket()`?먯꽌 諛붾줈 encode + framing + ring append瑜??섑뻾?쒕떎.
  - ??寃쎈줈??`m_sendRingMutex`濡?蹂댄샇?쒕떎.
- `OwnerThread`
  - `SendPacket()`? `FPacketBuffer*`留?owner queue???ｋ뒗??
  - ?ㅼ젣 encode + framing + ring append + send submit? owner worker媛 ?섑뻾?쒕떎.
  - ?곕씪??ring ?먯껜???⑥씪 ?ㅻ젅?쒓? 留뚯졇 lock???꾩슂 ?녿떎.

## 5. 寃利?寃곌낵
### 5-1. 60珥??ㅻえ??- `Direct`
  - [client.console.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_smoke\direct\client.console.log)
  - `echo validation succeeded. sessions=250 responses=6687`
- `OwnerThread`
  - [client.console.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_smoke\owner\client.console.log)
  - `echo validation succeeded. sessions=250 responses=6650`
- ????紐⑤몢 [server.console.err.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_smoke\direct\server.console.err.log), [server.console.err.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_smoke\owner\server.console.err.log) ??移섎챸 ?ㅻ쪟媛 ?놁뿀??

### 5-2. 5遺?怨좊???議곌굔:
- `250 sessions`
- `holdSeconds=300`
- `interval=0`
- `room-change=90%`
- race / sleep injection 鍮꾪솢??
寃곌낵:
- `Direct`
  - [client.console.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_5m\direct\client.console.log)
  - `echo validation succeeded. sessions=250 responses=26667`
- `OwnerThread`
  - [client.console.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_session_ring_5m\owner\client.console.log)
  - `echo validation succeeded. sessions=250 responses=25701`

異붽? ?뺤씤:
- ???쒕쾭 濡쒓렇 紐⑤몢 `send stall`, `packet exceeded max send packet size`, `RIOSend failed`媛 寃異쒕릺吏 ?딆븯??
- ?대쾲 議곌굔?먯꽑 `Direct`媛 `OwnerThread`蹂대떎 ?쎄컙 ?믪? 泥섎━?됱쓣 蹂댁???

### 5-3. ?꾩냽 寃利? 愿痢≪꽦/?덉쇅 寃쎈줈 ?뺣━
- `RIO Direct` 30珥?怨좊????ㅻえ?ъ뿉??ring ?ъ슜???듦퀎媛 ?ㅼ젣濡?李랁엳??寃껋쓣 ?뺤씤?덈떎.
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_114226_ring_obs_smoke\summary.csv)
  - [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_114226_ring_obs_smoke\rio_direct\server.log)
- `Direct` ?덉쇅 寃쎈줈 lock 踰붿쐞 異뺤냼 ?꾩뿉???뺤긽 寃쎈줈???좎??먮떎.
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_120411_direct_lockscope_smoke_retry\summary.csv)
- 媛뺤젣 ?덉쇅 ?ы쁽??紐⑤몢 ?깃났?덈떎.
  - oversize: [forced_rio_direct_oversize_20260407_121031](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_oversize_20260407_121031)
  - send stall: [forced_rio_direct_send_stall_strong_20260407_121147](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_send_stall_strong_20260407_121147)
- 利??꾩옱 ?쒖젏?먮뒗 珥덇린 由щ럭?먯꽌 ?④꺼?????곕젮媛 紐⑤몢 ?꾩냽 ?묒뾽?쇰줈 ?댁냼???곹깭??

## 6. 寃곕줎
- ?대쾲 `session send ring` ?꾪솚? ?꾩옱 ?뚯뒪??踰붿쐞?먯꽌 correctness? ?덉젙??湲곗???異⑹”?쒕떎.
- ?뱁엳 `per-send send buffer`, `per-send request context ?좊떦`, `per-send register/deregister` 寃쎈줈瑜?嫄룹뼱?대㈃?쒕룄 `Direct / OwnerThread` ?묒そ?먯꽌 ?뺤긽 ?숈옉???뺤씤?덈떎.
- 珥덇린 由щ럭?먯꽌 ?④꼈??`Direct` ring 愿痢≪꽦 怨듬갚怨??덉쇅 寃쎈줈 lock 踰붿쐞 ?곕젮???꾩냽 ?묒뾽?쇰줈 ?뺣━?먮떎.
- ?곕씪???ㅼ쓬 ?곗꽑?쒖쐞??湲곕뒫 ?섏젙???꾨땲??  - `OwnerThread` byte ?⑥쐞 backlog ?듦퀎 異붽?
  - `Direct` 誘몄꽭 理쒖쟻?붿? 愿痢≪꽦 湲곕컲 ?댁꽍
  - ?꾩슂 ??`OwnerThread` 寃쎈웾???ш????쒖쑝濡?蹂대뒗 寃껋씠 留욌떎.

