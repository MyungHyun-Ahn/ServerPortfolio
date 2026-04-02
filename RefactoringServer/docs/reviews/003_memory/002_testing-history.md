# Memory Pool Testing History

## 1. 臾몄꽌 ??븷
- ??臾몄꽌??`FLockFreeMemoryPool`, `FTlsMemoryPoolManager` 愿???뚯뒪??怨꾪쉷怨??ㅽ뻾 ?대젰????怨녹뿉 紐⑥븘 ??湲곕줉 臾몄꽌??
- ?꾩옱 援ъ“ ?먮떒怨??ㅺ퀎 洹쇨굅??[memory-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory\001_memory-pool-review.md)瑜??곗꽑?쒕떎.

## 2. ?뚯뒪?????- [`FLockFreeMemoryPool.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\Memory\FLockFreeMemoryPool.h)
- [`FTlsMemoryPool.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\Memory\FTlsMemoryPool.h)
- [`LockFreeTests/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\Main.cpp)
- [`TlsMemoryPoolSoakTest/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\TlsMemoryPoolSoakTest\Main.cpp)

## 3. 湲곕낯 寃利???ぉ
- `TLS memory pool parallel`
  - 8 thread 湲곗??쇰줈 batch alloc/free 諛섎났 ??`GetUseCount() == 0` 蹂듦?瑜??뺤씤?쒕떎.

## 4. soak ?뚯뒪????ぉ
- `TlsMemoryPoolSoakTest`
  - `--seconds`, `--threads`, `--batch-size`, `--report-seconds`, `--log-path`瑜?諛쏆븘 ?μ떆媛?alloc/free 臾닿껐?깆쓣 ?뺤씤?쒕떎.
- ?⑷꺽 湲곗?
  - 醫낅즺 肄붾뱶媛 0?대떎.
  - 理쒖쥌 `alloc == free`??
  - 理쒖쥌 `inUse == 0`?대떎.
  - 留덉?留?以꾩씠 `PASS`??

## 5. ?ㅽ뻾 湲곕줉
### 5-1. 2026-04-01 湲곕낯 湲곕뒫 ?뚯뒪??- `RefactoringServer.sln` x64 Debug 鍮뚮뱶 ?깃났
- [`LockFreeTests.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
  - `TLS memory pool parallel : PASS`
  - 醫낅즺 ??`GetUseCount() == 0`

### 5-2. 2026-04-01 吏㏃? soak ?뚯뒪??- 濡쒓렇: [`tls_sanity.log`](D:\Project\ServerPortfolio\GameServer\Out\tls_sanity.log)
- ?뺤씤 寃곌낵
  - 理쒖쥌 `alloc == free`
  - 理쒖쥌 `inUse == 0`
  - 留덉?留?以?`PASS`

### 5-3. 2026-04-01 ?μ떆媛?soak ?뚯뒪??- 濡쒓렇: [`tls_overnight.log`](D:\Project\ServerPortfolio\GameServer\Out\tls_overnight.log)
- ?뺤씤 寃곌낵
  - 理쒖쥌 `alloc = 124479256832`
  - 理쒖쥌 `free = 124479256832`
  - 理쒖쥌 `inUse = 0`
  - 理쒖쥌 `capacity = 1024`
  - 留덉?留?以?`PASS`

## 6. ?꾩옱 ?댁꽍
- ?꾩옱 ?뚯뒪??踰붿쐞?먯꽌??TLS local cache? shared pool 議고빀?먯꽌 紐낅갚???꾩닔 吏뺥썑瑜?蹂댁씠吏 ?딆븯??
- ?댄썑 `BucketSize`, `BucketCount`, `UseQueue` ?뺤콉??諛붽씀硫?癒쇱? `LockFreeTests`, 洹몃떎??`TlsMemoryPoolSoakTest`瑜?媛숈? 湲곗??쇰줈 ?ㅼ떆 ?ㅽ뻾?쒕떎.

