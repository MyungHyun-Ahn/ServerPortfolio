# RIO Direct Ring Observability

## 1. 紐⑹쟻
- `RIO Direct` 寃쎈줈??`session send ring` ?ъ슜?됱쓣 ?댁쁺 ?듦퀎?먯꽌 吏곸젒 愿李고븷 ???덇쾶 留뚮뱺??
- ?꾩옱 `queuedSendBuffers` 以묒떖 ?듦퀎??`OwnerThread` ??湲몄씠??蹂댁씠吏留? `Direct`???ㅼ젣 蹂묐ぉ??ring ?ъ슜 ?섏쐞????蹂댁씠吏 ?딅뒗??

## 2. 諛곌꼍
- `RIO Direct`???꾩옱 `session-local send ring`??諛붾줈 append?섍퀬, ?몄뀡??`in-flight send` 1媛쒕쭔 ?좎??쒕떎.
- 洹몃윭???꾩옱 stats??[FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp) ??`GetStatsSnapshot()` 湲곗??쇰줈 `GetQueuedSendBufferCount()` ?⑹궛留??쒓났?쒕떎.
- ??媛믪? [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.cpp) ??owner queue 以묒떖 媛믪씠??`Direct` ring pressure瑜??ㅻ챸?섏? 紐삵븳??

## 3. 紐⑺몴
- `Direct` 寃쎈줈?먯꽌 ?ㅼ쓬 媛믪쓣 理쒖냼??愿李?媛?ν븯寃?留뚮뱺??
  - `sendRingUsedBytes`
  - `sendRingInFlightBytes`
  - `maxObservedSendRingUsedBytes`
  - ?꾩슂 ??`sendRingFreeBytes`
- ?몄뀡 ?⑥쐞? ?쒕쾭 吏묎퀎 ?⑥쐞 ?????쒓났?쒕떎.
- ?듦퀎 異붽?媛 hot path瑜??ш쾶 ?먮━寃?留뚮뱾吏 ?딅룄濡??쒕떎.

## 4. 踰붿쐞
????뚯씪:
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.cpp)
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)
- ?꾩슂 ??[BackendTypes.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\BackendTypes.h)

鍮꾨쾾??
- send ring 援ъ“ ?먯껜 蹂寃?- `OwnerThread` ??寃쎈웾??- RTT 怨꾩륫 ?щ㎎ 蹂寃?
## 5. ?ㅺ퀎 諛⑺뼢
### 5-1. ?몄뀡 ?대? 怨꾩륫
- `FRioSession`??ring 愿???섏쐞瑜??쎌쓣 ???덈뒗 getter瑜?異붽??쒕떎.
- `TryAppendSendPacket()`, `CompleteCurrentSend()`, `CancelPreparedSend()`?먯꽌 ?꾩옱 ?섏쐞? 理쒕? ?섏쐞瑜?媛깆떊?쒕떎.
- 理쒕? ?섏쐞??monotonic max濡?愿由ы븳??

### 5-2. ?쒕쾭 吏묎퀎 ?듦퀎
- `GetStatsSnapshot()`?먯꽌 ?꾩껜 ?몄뀡???묒쑝硫??ㅼ쓬 吏묎퀎瑜?怨꾩궛?쒕떎.
  - `totalSendRingUsedBytes`
  - `maxObservedSendRingUsedBytes`
  - ?꾩슂 ??`totalSendRingInFlightBytes`
- 湲곗〈 `queuedSendBufferCount`???좎??섎릺, `RIO Direct` ?댁꽍?먮뒗 ring ?듦퀎瑜??곗꽑 李멸퀬?섎룄濡?臾몄꽌?뷀븳??

### 5-3. 濡쒓렇/?댁쁺 ?댁꽍
- `send stall` 吏곸쟾 ?곹깭瑜??댁꽍?????덈룄濡?ring ?섏쐞瑜?寃쎄퀬 濡쒓렇?먮룄 ?ы븿?쒕떎.
- 怨좊????곗뿉?쒕뒗 `queuedSendBuffers=0`?댁뼱??`ring used`媛 ?믨쾶 移섏넖?붿? ?뺤씤?????덉뼱???쒕떎.

## 6. 援ы쁽 ?쒖꽌
1. `FRioSession`??ring ?섏쐞 getter? 理쒕? ?섏쐞 怨꾩륫 異붽?
2. `SServerStats` ?먮뒗 ?숇벑 ?듦퀎 援ъ“??ring 愿???꾨뱶 異붽?
3. `FRioServer::GetStatsSnapshot()` 吏묎퀎 諛섏쁺
4. `send stall` 寃쎄퀬 濡쒓렇???꾩옱 used/free/in-flight ?섏쐞 異붽?
5. 吏㏃? ?ㅻえ????`Rio Direct` 5遺??곗쑝濡??섏튂媛 ?ㅼ젣濡?李랁엳?붿? ?뺤씤

## 7. 寃利?湲곗? 諛?寃곌낵
### 7-1. 寃利?湲곗?
- 鍮뚮뱶 ?깃났
- 湲곗〈 `RIO Direct` 湲곕뒫/?덉젙???뚭? ?놁쓬
- `Direct` ?곗뿉??`sendRingUsedBytes` 怨꾩뿴 ?섏튂媛 0留?李랁엳吏 ?딄퀬 ?ㅼ젣濡?蹂?쒕떎
- `send stall` 諛쒖깮 ??濡쒓렇留뚯쑝濡??뱀떆 ring ?곹깭瑜??댁꽍?????덈떎

### 7-2. 援ы쁽 諛?寃利?寃곌낵
- 援ы쁽 ?꾨즺.
- `FRioSession`???ㅼ쓬 愿痢≪꽦 媛믪쓣 異붽??덈떎.
  - `sendRingUsedBytes`
  - `sendRingInFlightBytes`
  - `maxObservedSendRingUsedBytes`
- `FRioServer::GetStatsSnapshot()`怨?headless stats 濡쒓렇???ㅼ쓬 ?쒕쾭 吏묎퀎 媛믪쓣 異붽??덈떎.
  - `totalSendRingUsedBytes`
  - `totalSendRingInFlightBytes`
  - `maxSessionSendRingUsedBytes`
  - `maxObservedSessionSendRingUsedBytes`
- `send stall` 寃쎄퀬 濡쒓렇?먮룄 ?몄뀡蹂?`used/free/in-flight/maxObserved` ?섏쐞瑜??④퍡 ?④린?꾨줉 諛섏쁺?덈떎.
- ?꾩껜 `Debug x64` 鍮뚮뱶 ?깃났.
- `Rio Direct` 30珥?怨좊????ㅻえ???깃났.
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_114226_ring_obs_smoke\summary.csv)
  - `responses=1006195`
  - `AvgSendTPS=16507.2950819672`
  - `EchoAvgMs=7.21`
- [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_30s_20260407_114226_ring_obs_smoke\rio_direct\server.log) ?먯꽌 `queuedSendBuffers=0`?댁뼱??`totalSendRingUsedBytes`, `totalSendRingInFlightBytes`, `maxObservedSessionSendRingUsedBytes`媛 ?ㅼ젣 媛믪쑝濡?蹂?섎뒗 寃껋쓣 ?뺤씤?덈떎.

## 8. 湲곕? ?④낵
- `Direct`???ㅼ젣 蹂묐ぉ??蹂댁씠吏 ?딄쾶 留뚮뱶??愿痢≪꽦 怨듬갚??以꾩씤??
- ?댄썑 `Direct` 誘몄꽭 理쒖쟻?붾굹 stall ?먯씤 遺꾩꽍???곗씠??湲곕컲?쇰줈 吏꾪뻾?????덈떎.

