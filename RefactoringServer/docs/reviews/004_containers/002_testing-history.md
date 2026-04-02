# Lock-Free Containers Testing History

## 1. 臾몄꽌 ??븷
- ??臾몄꽌??`FLockFreeQueue`, `FLockFreeStack` 愿???뚯뒪??怨꾪쉷怨??ㅽ뻾 ?대젰????怨녹뿉 紐⑥븘 ??湲곕줉 臾몄꽌??
- ?꾩옱 援ъ“ ?먮떒怨??ㅺ퀎 洹쇨굅??[lock-free-containers-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers\001_lock-free-containers-review.md)瑜??곗꽑?쒕떎.

## 2. ?뚯뒪?????- [`FLockFreeQueue.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\Containers\FLockFreeQueue.h)
- [`FLockFreeStack.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\Containers\FLockFreeStack.h)
- [`LockFreeTests/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\Main.cpp)
- [`LockFreeQueueSoakTest/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\LockFreeQueueSoakTest\Main.cpp)

## 3. 湲곕낯 寃利???ぉ
- `Queue linear FIFO`
  - ?⑥씪 ?ㅻ젅?쒖뿉??1~1000 enqueue/dequeue ?쒖꽌媛 ?좎??섎뒗吏 ?뺤씤?쒕떎.
- `Queue parallel sum`
  - 4 producer, 4 consumer 湲곗??쇰줈 珥?媛쒖닔? ?⑷퀎媛 ?쇱튂?섎뒗吏 ?뺤씤?쒕떎.
- `Stack parallel sum`
  - 4 producer, 4 consumer 湲곗??쇰줈 珥?媛쒖닔? ?⑷퀎媛 ?쇱튂?섎뒗吏 ?뺤씤?쒕떎.

## 4. soak ?뚯뒪????ぉ
- `LockFreeQueueSoakTest`
  - `--seconds`, `--producers`, `--consumers`, `--report-seconds`, `--log-path`瑜?諛쏆븘 ?μ떆媛??앹궛/?뚮퉬 臾닿껐?깆쓣 ?뺤씤?쒕떎.
- ?⑷꺽 湲곗?
  - 醫낅즺 肄붾뱶媛 0?대떎.
  - 理쒖쥌 `produced == consumed`?대떎.
  - 理쒖쥌 `producedSum == consumedSum`?대떎.
  - 留덉?留?以꾩씠 `PASS`??

## 5. ?ㅽ뻾 湲곕줉
### 5-1. 2026-04-01 湲곕낯 湲곕뒫 ?뚯뒪??- `RefactoringServer.sln` x64 Debug 鍮뚮뱶 ?깃났
- [`LockFreeTests.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
  - `Queue linear FIFO : PASS`
  - `Queue parallel sum : PASS`
  - `Stack parallel sum : PASS`

### 5-2. 2026-04-01 吏㏃? soak ?뚯뒪??- 濡쒓렇: [`queue_sanity.log`](D:\Project\ServerPortfolio\GameServer\Out\queue_sanity.log)
- ?뺤씤 寃곌낵
  - 理쒖쥌 `produced == consumed`
  - 理쒖쥌 `producedSum == consumedSum`
  - 留덉?留?以?`PASS`

### 5-3. 2026-04-01 ?μ떆媛?soak ?뚯뒪??- 濡쒓렇: [`queue_overnight.log`](D:\Project\ServerPortfolio\GameServer\Out\queue_overnight.log)
- ?뺤씤 寃곌낵
  - 理쒖쥌 `produced = 60423450219`
  - 理쒖쥌 `consumed = 60423450219`
  - 理쒖쥌 `producedSum = 17715748990661240722`
  - 理쒖쥌 `consumedSum = 17715748990661240722`
  - 留덉?留?以?`PASS`

## 6. ?꾩옱 ?댁꽍
- ?꾩옱 ?뚯뒪??踰붿쐞?먯꽌??queue? stack 紐⑤몢 利됱떆 ?쒕윭?섎뒗 臾닿껐???ㅻ쪟瑜?蹂댁씠吏 ?딆븯??
- ?댄썑 而⑦뀒?대꼫 援ъ“瑜??섏젙?섎㈃ 癒쇱? `LockFreeTests`, 洹몃떎??`LockFreeQueueSoakTest`瑜?媛숈? 湲곗??쇰줈 ?ㅼ떆 ?ㅽ뻾?쒕떎.

