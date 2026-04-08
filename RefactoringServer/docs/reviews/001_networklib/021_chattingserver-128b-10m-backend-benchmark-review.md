# ChattingServer 128B 10M Backend Benchmark Review

## 1. 紐⑹쟻
- `ChattingServer`?먯꽌 ?쇰컲?곸씤 梨꾪똿 硫붿떆吏 ?ш린??媛源뚯슫 `128B payload` 湲곗??쇰줈 `IOCP`, `RIO Direct`, `RIO OwnerThread`瑜?`10遺? ?숈븞 鍮꾧탳??寃곌낵瑜??뺣━?쒕떎.
- ?대쾲 臾몄꽌??`8KiB` ???⑦궥 stress test? 遺꾨━?댁꽌, ?ㅼ젣 梨꾪똿??媛源뚯슫 議곌굔?먯꽌 ?대뼡 諛깆뿏?쒓? ???좊━?쒖? ?뺤씤?섎뒗 ??紐⑹쟻???덈떎.

## 2. 議곌굔
- manifest:
  - [chatting-rio-vs-iocp-128b-10m.yaml](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\manifests\chatting-rio-vs-iocp-128b-10m.yaml)
- 寃곌낵 猷⑦듃:
  - [20260407_181046_rio_vs_iocp_128b_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m)
- 怨듯넻 ?ㅼ젙:
  - `MeasureSeconds = 600`
  - `PayloadSizeBytes = 128`
  - `MaxChatPayloadBytes = 256`
  - `SessionCount = 150`
  - `ConnectsPerSecond = 50`
  - `SendIntervalMs = 0`
  - `RoomSelectionMode = Hotspot`
  - `HotspotRoomIds = 77`
  - `HotspotBiasPercent = 90`
  - `RoomCount = 50`
  - `RoomCapacity = 256`
  - `RioSendRingSizeBytes = 65536`
- ?ㅽ뻾 寃곌낵:
  - [sequence-summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\sequence-summary.csv)
  - [iocp_default_128b_150_hotspot_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\iocp_default_128b_150_hotspot_10m)
  - [rio_direct_128b_150_hotspot_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_direct_128b_150_hotspot_10m)
  - [rio_owner_128b_150_hotspot_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_owner_128b_150_hotspot_10m)

## 3. 痢≪젙 湲곗?
- 二?泥섎━??吏?쒕뒗 `chattingSuccess / elapsedSeconds`濡?怨꾩궛?쒕떎.
  - ?대쾲 ?붾???`ChattingRq`瑜?蹂대궦 ??`ChattingRp`瑜?湲곕떎由щ뒗 援ъ“?? 珥??깃났 嫄댁닔瑜??꾩껜 ?쒓컙?쇰줈 ?섎늿 媛믪씠 媛??鍮꾧탳?섍린 ?쎈떎.
  - 愿??肄붾뱶:
    - [Main.cpp#L1135](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingDummyClient\Main.cpp#L1135)
    - [Main.cpp#L1138](D:\Project\ServerPortfolio\RefactoringServer\Chatting\ChattingDummyClient\Main.cpp#L1138)
- `broadcast avg/s`??`broadcastReceive / elapsedSeconds`濡?怨꾩궛?쒕떎.
- RTT??媛?`rtt.csv`?먯꽌 `stage = chatting-response`??**留덉?留??꾩쟻 ??* 湲곗? `overall_avg_ms`瑜??ъ슜?쒕떎.
  - [iocp rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\iocp_default_128b_150_hotspot_10m\rtt.csv)
  - [rio_direct rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_direct_128b_150_hotspot_10m\rtt.csv)
  - [rio_owner rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_owner_128b_150_hotspot_10m\rtt.csv)
- `sequence-summary.csv`??`sendTPS`, `recvTPS`, `cpuPercent`??醫낅즺 ?쒖젏 snapshot ?깃꺽???덉쑝誘濡?蹂댁“ 吏?쒕줈留??ъ슜?쒕떎.

## 4. 寃곌낵 ?붿빟
| Mode | chattingSuccess | chat avg/s | broadcastReceive | broadcast avg/s | chatting RTT avg | chatting RTT max1 | reconnect | unexpectedDisconnect |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `IOCP` | `91,831` | `153.05` | `10,288,760` | `17,147.36` | `959.506 ms` | `1388.835 ms` | `0` | `0` |
| `RIO Direct` | `99,324` | `165.53` | `11,016,743` | `18,360.63` | `887.294 ms` | `1065.032 ms` | `0` | `0` |
| `RIO OwnerThread` | `88,142` | `146.90` | `9,871,537` | `16,452.01` | `999.514 ms` | `1080.473 ms` | `0` | `0` |

遺媛 吏??

| Mode | final cpuPercent | final workingSetMB | 愿李곕맂 send ring ?ъ슜??|
| --- | ---: | ---: | --- |
| `IOCP` | `0.38%` | `28.73 MB` | ?대떦 ?놁쓬 |
| `RIO Direct` | `3.61%` | `27.81 MB` | 濡쒓렇 湲곗? `maxObservedSessionSendRingUsedBytes = 1217` |
| `RIO OwnerThread` | `0.67%` | `37.81 MB` | 濡쒓렇 湲곗? `maxObservedSessionSendRingUsedBytes = 2324` |

濡쒓렇 洹쇨굅:
- [rio_direct server.stdout.log](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_direct_128b_150_hotspot_10m\server.stdout.log)
- [rio_owner server.stdout.log](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_owner_128b_150_hotspot_10m\server.stdout.log)

## 5. ?댁꽍
### 5-1. ?대쾲 10遺?議곌굔?먯꽌??`RIO Direct`媛 媛??醫뗫떎
- `chat avg/s` 湲곗??쇰줈 `RIO Direct`??`IOCP` ?鍮???`8.15%` ?믩떎.
- `broadcast avg/s`??`RIO Direct`媛 媛???믩떎.
- RTT??`RIO Direct`媛 媛????떎.
  - `IOCP` ?鍮??됯퇏 RTT ??`7.53%` 媛쒖꽑
  - `RIO OwnerThread` ?鍮??됯퇏 RTT ??`11.23%` 媛쒖꽑

### 5-2. `RIO OwnerThread`???덉젙?곸씠吏留? ?대쾲 議곌굔?먯꽌??handoff 鍮꾩슜?????ш쾶 蹂댁씤??- `RIO Direct`???≪떊 ?몄텧 ?쒖젏??諛붾줈 ring append ??`PostSend`濡??댁뼱吏꾨떎.
  - [FRioServer.cpp#L234](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L234)
  - [FRioServer.cpp#L239](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L239)
- `RIO OwnerThread`??癒쇱? owner queue???곸옱???? owner worker媛 ?섏쨷??drain ?섎㈃??`AppendPacketToSendRing`怨?`PostSend`瑜??섑뻾?쒕떎.
  - [FRioServer.cpp#L234](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L234)
  - [FRioServer.cpp#L803](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L803)
  - [FRioServer.cpp#L815](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp#L815)
- ?대쾲 ?붾????몄뀡??`1 outstanding chat` 援ъ“?쇱꽌, `ChattingRp`媛 ??뼱吏硫??ㅼ쓬 `ChattingRq`????뼱吏꾨떎.
  - 利?per-message handoff 鍮꾩슜??RTT? 泥섎━?됱뿉 吏곸젒 諛섏쁺?쒕떎.

### 5-3. `128B`?먯꽌??send ring??蹂묐ぉ???꾨땲?덈떎
- ??紐⑤뱶 紐⑤몢 `reconnect = 0`, `unexpectedDisconnect = 0`, `timeout = 0`, `permanentFailure = 0`?쇰줈 ?앸궗??
- `RIO OwnerThread` 濡쒓렇?먮룄 `RIO send stall detected`媛 ?놁뿀??
  - [rio_owner server.stdout.log](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_owner_128b_150_hotspot_10m\server.stdout.log)
- 愿李곕맂 ring ?ъ슜?됰룄 `1217B`, `2324B` ?섏??대씪 `64KiB` ring? 異⑸텇?덈떎.
- ?곕씪???대쾲 寃곌낵 李⑥씠??`send ring ?⑸웾 遺議?蹂대떎 `dispatch path 李⑥씠`? `worker scheduling` ?곹뼢?쇰줈 蹂대뒗 ?몄씠 留욌떎.

## 6. 二쇱쓽
- ?댁쟾??`rtt.csv`??泥?踰덉㎏ `chatting-response` ?됰쭔 蹂대㈃ `OwnerThread`媛 ????븘 蹂댁씪 ???덉뿀?붾뜲, 洹?媛믪? 珥덇린 1遺??꾩쟻媛믪씪 肉먯씠??
- 理쒖쥌 鍮꾧탳??諛섎뱶??`rtt.csv`??留덉?留?`chatting-response` ?됱쓣 湲곗??쇰줈 ?댁빞 ?쒕떎.
- 媛숈? ?댁쑀濡?`sequence-summary.csv`??留덉?留?`sendTPS` ?섎굹留?蹂닿퀬 ?꾩껜 泥섎━???곗뿴???먮떒?섎㈃ ?쒓끝?????덈떎.

## 7. 寃곕줎
- `128B / 150 sessions / hotspot 90% / 10遺? 議곌굔?먯꽌??`RIO Direct`媛 媛??醫뗭? 洹좏삎??蹂댁???
  - 媛???믪? `chat avg/s`
  - 媛???믪? `broadcast avg/s`
  - 媛????? `chatting-response RTT`
- `RIO OwnerThread`?????⑦궥 stress ?곹솴怨??щ━ ?대쾲 議곌굔?먯꽌 ?덉젙??臾몄젣???놁뿀吏留? 泥섎━?됯낵 RTT 紐⑤몢 `RIO Direct`蹂대떎 ?ㅼ쿂議뚮떎.
- ?곕씪???욎쑝濡?`?쇰컲 梨꾪똿 ?ш린` 湲곗? 鍮꾧탳??baseline? ?대쾲 `128B 10遺? 寃곌낵濡??먭퀬, `8KiB`??蹂꾨룄??stress / send-ring ?쒓퀎 寃利앹쑝濡?遺꾨━?댁꽌 蹂대뒗 寃껋씠 ?곸젅?섎떎.

## 8. ?ㅼ쓬 ?≪뀡
- `128 / 256 / 512 / 1024B` payload sweep
- `SessionCount` sweep
- `RIO OwnerThread`媛 遺덈━?댁???援ш컙?먯꽌 owner queue backlog? worker scheduling 吏??異붽?


