# RIO Direct Failure Path Lock Scope Reduction

## 1. 紐⑹쟻
- `RIO Direct`??鍮꾩젙??寃쎈줈?먯꽌 `sendRingMutex`瑜?伊?梨?`CloseSession()`源뚯? ?ㅼ뼱媛??援ъ“瑜??뺣━?쒕떎.
- ?뺤긽 寃쎈줈 ?깅뒫蹂대떎?? ?덉쇅 寃쎈줈??lock 踰붿쐞瑜?以꾩뿬 肄붾뱶 ?댁꽍?깃낵 ?덉쟾?깆쓣 ?믪씠??寃껋씠 紐⑺몴??

## 2. 諛곌꼍
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp) ??`AppendPacketToSendRing()`??`Direct` 紐⑤뱶?먯꽌 `m_sendRingMutex`瑜??↔퀬
  - encode
  - framing
  - ring append
  - oversize / ring full fail-fast ?먯젙
???섑뻾?쒕떎.
- ?꾩옱??`packet > 8KiB` ?먮뒗 `ring full`????lock ?덉뿉??諛붾줈 `CloseSession()`???몄텧?쒕떎.
- ?뺤콉??fail-fast ?먯껜??留욎?留? ?붿껌 ???뚯폆 ?뺣━源뚯? ?꾧퀎援ъ뿭???ㅼ뼱媛 ?덉쇅 寃쎈줈 ?댁꽍??遺덊븘?뷀븯寃?臾닿쾪??

## 3. 紐⑺몴
- `sendRingMutex` ?덉뿉?쒕뒗 ring ?곹깭 ?먯젙怨??ㅽ뙣 留덊궧源뚯?留??섑뻾?쒕떎.
- ?ㅼ젣 `CloseSession()` ?몄텧? lock 諛뽰쑝濡?類??
- ?꾩옱??fail-fast ?뺤콉? ?좎??쒕떎.
  - `packet > 8KiB` => 鍮꾩젙??  - `64KiB ring full` => `send stall` 鍮꾩젙??
## 4. 踰붿쐞
????뚯씪:
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)
- ?꾩슂 ??[FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)
- ?꾩슂 ??[FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.cpp)

鍮꾨쾾??
- `Direct`瑜?lock-free濡?諛붽씀???묒뾽
- `OwnerThread` 寃쎈줈 蹂寃?- send ring ?ш린/?뺤콉 蹂寃?
## 5. ?ㅺ퀎 諛⑺뼢
### 5-1. ?ㅽ뙣 ?댁쑀瑜?癒쇱? ?뺤젙
- `AppendPacketToSendRing()` ?대??먯꽌 ?ㅽ뙣 ?댁쑀瑜?enum ?먮뒗 local state濡?遺꾨━?쒕떎.
  - `None`
  - `OversizePacket`
  - `SendStall`
  - ?꾩슂 ??`BuildPacketPartsFailed`

### 5-2. lock ??/ lock 諛?遺꾨━
- lock ??
  - packet size ?먯젙
  - ring append ?쒕룄
  - ?ㅽ뙣 ?댁쑀 湲곕줉
- lock 諛?
  - 寃쎄퀬 濡쒓렇 異쒕젰
  - `CloseSession()` ?몄텧

### 5-3. ?뺤콉 ?좎?
- lock 踰붿쐞瑜?以꾩뿬???뺤콉? 洹몃?濡??좎??쒕떎.
- 利?retry/backpressure瑜??덈줈 留뚮뱾吏 ?딄퀬, 湲곗〈??fail-fast??洹몃?濡??붾떎.

## 6. 援ы쁽 ?쒖꽌
1. `AppendPacketToSendRing()`???ㅽ뙣 寃쎈줈瑜?local result enum?쇰줈 遺꾨━
2. `sendRingMutex` 援ш컙 諛뽰쑝濡?`CloseSession()` ?대룞
3. 濡쒓렇??媛?ν븯硫?lock 諛뽰뿉??理쒖쥌 異쒕젰
4. `RIO Direct` ?ㅻえ?ъ? 媛뺤젣 ?덉쇅 ?ы쁽?쇰줈 fail-fast ?좎? ?щ? ?뺤씤

## 7. 寃利?湲곗? 諛?寃곌낵
### 7-1. 寃利?湲곗?
- 鍮뚮뱶 ?깃났
- ?뺤긽 寃쎈줈 throughput/RTT ?뚭? ?놁쓬
- oversize / ring full 媛뺤젣 ?ы쁽 ??  - ?몄뀡? ?ъ쟾??fail-fast濡??ロ옒
  - `CloseSession()`??lock 諛뽰뿉???몄텧??- deadlock, ?ъ쭊?? ?댁쨷 close 吏뺥썑 ?놁쓬

### 7-2. 援ы쁽 諛?寃利?寃곌낵
- 援ы쁽 ?꾨즺.
- `AppendPacketToSendRing()`? ?댁젣 `sendRingMutex` ?덉뿉?쒕뒗 ?ㅽ뙣 ?댁쑀留??먯젙?섍퀬, ?ㅼ젣 寃쎄퀬 濡쒓렇 異쒕젰怨?`CloseSession()` ?몄텧? lock 諛뽰뿉???섑뻾?쒕떎.
- ?꾩껜 `Debug x64` 鍮뚮뱶 ?깃났.
- `Rio Direct` 30珥?怨좊????ъ떆?????깃났.
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_120411_direct_lockscope_smoke_retry\summary.csv)
  - `responses=1010880`
  - `AvgSendTPS=16860.5`
  - `EchoAvgMs=7.181`
- `OversizePacket` 媛뺤젣 ?ы쁽 ?깃났.
  - 寃곌낵 ?대뜑: [forced_rio_direct_oversize_20260407_121031](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_oversize_20260407_121031)
  - ?뺤씤 濡쒓렇: [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_oversize_20260407_121031\server.log)
  - ?듭떖 濡쒓렇: `RIO send rejected because packet exceeded max send packet size`
- `SendStall` 媛뺤젣 ?ы쁽 ?깃났.
  - 寃곌낵 ?대뜑: [forced_rio_direct_send_stall_strong_20260407_121147](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_send_stall_strong_20260407_121147)
  - ?뺤씤 濡쒓렇: [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\forced_rio_direct_send_stall_strong_20260407_121147\server.log)
  - ?듭떖 濡쒓렇: `RIO send stall detected because session send ring was full`
  - ?④퍡 湲곕줉??愿痢≪꽦 媛?
    - `ringBytes=65536`
    - `packetBytes=7018`
    - `usedBytes=63162`
    - `freeBytes=2374`
    - `inFlightBytes=14036`
- 泥?踰덉㎏ ?쏀븳 `send stall` ?쒕굹由ъ삤?먯꽌???ы쁽?섏? ?딆븯怨? ??媛뺥븳 議곌굔?먯꽌 ?뺤긽?곸쑝濡?fail-fast 寃쎈줈瑜??붾떎.

## 8. 湲곕? ?④낵
- ?덉쇅 寃쎈줈??lock 踰붿쐞瑜?以꾩뿬 肄붾뱶 ?댄빐媛 ?ъ썙吏꾨떎.
- ?댄썑 愿痢≪꽦/吏꾨떒 濡쒓렇瑜??ｌ쓣 ?뚮룄 lock 援ш컙?????ㅼ뿼?쒗궓??
- ?뺤긽 寃쎈줈 理쒖쟻?붿? 遺꾨━?? ?덉쟾???뺣━ ?묒뾽?쇰줈 吏꾪뻾?????덈떎.

