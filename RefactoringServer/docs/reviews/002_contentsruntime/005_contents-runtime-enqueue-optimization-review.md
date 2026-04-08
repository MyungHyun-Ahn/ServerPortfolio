# ContentsRuntime enqueue 寃쎈줈 理쒖쟻??由щ럭

## 1. 紐⑹쟻
- hot path 遺꾩꽍 寃곌낵瑜?諛뷀깢?쇰줈 ?ㅼ젣 contention 吏?먯쓣 以꾩???
- ?곸슜 ??곸? ??媛吏???
  1. `FContentRuntime::EnqueuePacket` 寃쎈웾??  2. `FContentThread` packet inbox lock-free ?꾨줈?좏???
## 2. ?곸슜 ?댁슜

### 2.1 `FContentRuntime::EnqueuePacket` 寃쎈웾??- ?뚯씪:
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)
- 蹂寃???
  - `sessionId -> contentId` 議고쉶媛 `unordered_map + mutex` 湲곕컲
  - packet留덈떎 ?꾩뿭 ?쎌쓣 嫄곗튂硫??쇱슦??議고쉶
- 蹂寃???
  - `sessionId` ?섏쐞 鍮꾪듃瑜?slot index濡??ъ슜?섎뒗 route table ?꾩엯
  - packet hot path??route table 吏곸젒 議고쉶
  - runtime ?쎌? `shared_mutex` 湲곕컲?쇰줈 議곗젙?섍퀬, packet enqueue??`shared_lock` 寃쎈줈 ?ъ슜

### 2.2 packet inbox lock-free ?꾨줈?좏???- ?뚯씪:
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.cpp)
- 蹂寃???
  - `packetQueue`媛 `std::deque + mutex`
- 蹂寃???
  - `FLockFreeQueue<SQueuedOwnedPacket*>` 湲곕컲 packet inbox 異붽?
  - queue item? TLS pool ?ъ궗??  - producer?????놁씠 enqueue
  - consumer??frame loop?먯꽌 drain ??湲곗〈 owned packet 泥섎━ 猷⑦봽 ?ъ궗??
## 3. ?섎룎由ш린 諛⑸쾿
- `packet inbox` lock-free???꾨줈?좏??낆씠???쎄쾶 ?섎룎由????덇쾶 ?좎??덈떎.
- ?좉? ?꾩튂:
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.cpp)
- ?ㅼ쐞移?
  - `kUseLockFreePacketInboxPrototype`
- `false`濡?諛붽씀硫?湲곗〈 `std::deque + mutex` 寃쎈줈濡?諛붾줈 蹂듦??쒕떎.

## 4. ?섏튂 鍮꾧탳
- 蹂寃???  - `runtimeEnqueueLockUs=88376.00`
  - `runtimeEnqueueMaxLockUs=2470.40`
  - `echoPacketEnqueueLockUs=64276.10`
  - `echoPacketEnqueueMaxLockUs=1699.80`
- 蹂寃???  - `runtimeEnqueueLockUs=19765.10`
  - `runtimeEnqueueMaxLockUs=53.40`
  - `echoPacketEnqueueLockUs=0.00`
  - `echoPacketEnqueueMaxLockUs=0.00`

## 5. 湲곕뒫 寃利?- ?⑤컻 ?ㅻえ???듦낵
  - [contents_lockfree_smoke2_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_lockfree_smoke2_client.log)
  - `echo validation succeeded.`
- 諛섎났 ?ㅻえ???듦낵
  - [contents_lockfree_smoke3_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_lockfree_smoke3_client.log)
  - `echo validation succeeded. sessions=1 responses=6 ... holdSeconds=1`

## 6. ?μ떆媛??덉젙??寃곌낵
- 6?쒓컙 race injection 寃利??듦낵
  - [launcher_6h_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\launcher_6h_20260403_032550.log)
  - [client_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\client_20260403_032550.log)
  - [server_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\server_20260403_032550.log)
- ?듭떖 寃곌낵
  - `echo validation succeeded. sessions=100 responses=10375394 ... holdSeconds=21600`
  - 留덉?留?援ш컙?먯꽌??`enqueueFailTPS=0`
  - `echoPacketEnqueueLockUs=0.00`
  - ?몄뀡?ㅼ? ?뺤긽?곸쑝濡?`client disconnected`, `echo content leave`, `Session closed`濡??뺣━??
## 7. 寃곕줎
- 1?④퀎 寃쎈웾?붿쓽 二??④낵??`FContentRuntime::EnqueuePacket`???
- 2?④퀎 lock-free ?꾨줈?좏??낆? packet inbox 寃쏀빀???ъ떎???쒓굅?덈떎.
- ?꾩옱 湲곗??쇰줈??  - route table 湲곕컲 runtime enqueue
  - lock-free packet inbox prototype
  議고빀??異⑸텇???덉젙?곸쑝濡?蹂댁씤??
- ?ㅻ쭔 ?꾩쟾??理쒖쥌 寃곕줎?대씪湲곕낫?? ?꾩옱 ?꾨줈?앺듃 ?④퀎?먯꽌 ?ㅼ궗??媛?ν븳 ?섏??쇰줈 ?먮떒?????덈떎.

