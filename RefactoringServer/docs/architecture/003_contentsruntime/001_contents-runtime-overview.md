# ContentsRuntime Overview

## 0. 理쒖떊 ?곹깭
- `2026-04-06` 湲곗? `ContentsRuntime`???뺤떇 援ъ“??`content-owned mailbox + worker executor`??
- `content instance = dedicated thread` 援ъ“???쒓굅?먮떎.
- `delegate / work stealing`??worker-global queue replay 諛⑹떇???꾨땲??`mailbox owner transfer` 諛⑹떇?쇰줈 援ы쁽?먮떎.

## 1. ?듭떖 援ъ꽦?붿냼
### FContentRuntime
- ?꾩튂: [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.h)
- ??븷
  - content registry
  - session route table
  - worker pool ?앹꽦怨?諛곗튂
  - enter / leave / move / packet 吏꾩엯 泥섎━
  - owner transfer request / commit

### FContentThread
- ?꾩튂: [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.h)
- ??븷
  - worker thread 1媛쒕? ?뚮━??executor
  - ready content??mailbox drain
  - due frame ?ㅽ뻾
  - boundary?먯꽌 transfer commit

### SContentExecutionState
- content instance媛 ?ㅼ젣濡??뚯쑀?섎뒗 ?ㅽ뻾 ?곹깭
- ?ы븿
  - mailbox
  - frame timing
  - queue depth
  - `inFlightCallbackCount`
  - `ownerWorkerIndex`
  - `requestedTransferTargetWorkerIndex`
  - transfer 愿??timestamp? ?듦퀎

### SSessionRoute
- session???꾩옱 route? move pending ?곹깭瑜?媛吏꾨떎.
- ?ы븿
  - `contentId`
  - `contentInstanceId`
  - `workerIndex`
  - `worker`
  - `moveState`
  - `pendingTarget*`
  - `pendingPackets`

## 2. queue / mailbox 紐⑤뜽
- `Enter / Leave / Packet`? worker-global queue媛 ?꾨땲??content mailbox???볦씤??
- worker??ready queue?먯꽌 content瑜?怨⑤씪 洹?mailbox瑜?batch濡?鍮꾩슫??
- `enterQueueDepth`, `leaveQueueDepth`, `packetQueueDepth`??蹂꾨룄 臾쇰━ queue媛 ?꾨땲??mailbox ?대? work 醫낅쪟蹂?depth ?듦퀎??

## 3. 諛곗튂 紐⑤뜽
- `FContentRuntime::Start()`?먯꽌 `ContentsWorkerThreadCount` 湲곗??쇰줈 worker瑜?留뚮뱺??
- ?깅줉??content instance??湲곕낯?곸쑝濡?round-robin?쇰줈 worker??諛곗튂?쒕떎.
- worker???ㅽ뻾?먯씠怨? mailbox? ?ㅽ뻾 ?곹깭???ㅼ냼?좎옄??content instance??

## 4. session ?먮쫫
### Enter
1. target content instance 寃곗젙
2. route ?ㅼ젙
3. target mailbox??`Enter` enqueue

### Leave
1. ?꾩옱 route 議고쉶
2. route ?뺣━
3. source mailbox??`Leave` enqueue

### Packet
1. current route 議고쉶
2. move 以묒씠 ?꾨땲硫?current mailbox??`Packet` enqueue
3. move 以묒씠硫?`pendingPackets`??hold

## 5. move ?먮쫫
### 怨듯넻 洹쒖튃
- route瑜?利됱떆 target?쇰줈 諛붽씀吏 ?딅뒗??
- 癒쇱? `Pending` ?곹깭瑜?留뚮뱾怨?target metadata留?route??湲곕줉?쒕떎.
- move 以?packet? `pendingPackets`??hold?덈떎媛 route commit ??replay?쒕떎.

### cross-worker move
1. source `Leave`
2. target `Enter`
3. target enter completion callback?먯꽌 route commit
4. held packet replay

### same-worker move
- fast-path瑜??ъ슜?쒕떎.
- source leave? target enter瑜??쇰컲 寃쎈줈蹂대떎 吏㏐쾶 ?곌껐?섏?留? route commit ?쒖젏? ?ъ쟾??target enter completion 湲곗??대떎.

## 6. owner transfer 湲곕컲 delegate / work stealing
### 怨듯넻 ?먯튃
- ??湲곕뒫 紐⑤몢 queue瑜???린吏 ?딅뒗??
- mailbox consumer??owner worker留?諛붽씔??
- ?ㅼ젣 commit? work boundary?먯꽌留??쇱뼱?쒕떎.

### delegate
- 諛붿걶 source worker媛 ?뱀젙 content瑜??ㅻⅨ worker濡?push?쒕떎.

### work stealing
- idle worker媛 諛붿걶 worker??content ?섎굹瑜?pull?쒕떎.

### 蹂댄샇 洹쒖튃
- allowList content留????- 以묐났 request 湲덉?
- request / commit cooldown
- source worker stabilization window
- pending move source/target content??transfer 湲덉?

## 7. ?덉쟾??洹쒖튃
- single-consumer handoff
- boundary-only commit
- move pending packet hold / replay
- pending move source/target content transfer 湲덉?

??洹쒖튃?쇰줈
- `move route commit committed=0`
- pending packet 怨좎갑
- move? transfer ?숈떆 異⑸룎
寃쎈줈瑜?留됰뒗??

## 8. 寃利?湲곗?
- `250 sessions`
- `connectsPerSecond=10`
- `interval=0`
- `room-change=90%`
- `Room 77 OnFrame Sleep 15ms`

?깃났 ??
- mailbox baseline 10遺?  - [content_mailbox_250x10m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x10m_room90)
- owner transfer policy 3遺?  - [owner_transfer_policy_loaded_3m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_3m)
- owner transfer policy 10遺?  - [owner_transfer_policy_loaded_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_10m)

## 9. 寃곕줎
- ?꾩옱 `ContentsRuntime`??`mailbox 以묒떖 援ъ“`濡??댄빐?섎뒗 寃껋씠 留욌떎.
- worker??queue owner媛 ?꾨땲??executor??
- `delegate / work stealing`???댁젣 mailbox owner transfer ?꾩뿉???덉쟾?섍쾶 ?숈옉?섎뒗 ?뺤옣 湲곕뒫?쇰줈 ?뺣━?먮떎.

