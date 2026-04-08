# Pure RIO Baseline Review

## 1. 紐⑹쟻
- `NetworkLib`???쒖닔 `RIO` backend瑜??ㅼ젣濡??숈옉?섎뒗 baseline?쇰줈 異붽???寃곌낵瑜??뺣━?쒕떎.
- ?대쾲 ?④퀎??`IOCP + RIO` ?섏씠釉뚮━?쒓? ?꾨땲??`RIO_EVENT_COMPLETION` 湲곕컲 ?쒖닔 `RIO` 援ы쁽?대떎.

## 2. ?대쾲 踰붿쐞
- [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.cpp)

?듭떖 援ы쁽:
- worker蹂?`RIO_CQ + event + owner thread`
- `AcceptEx + WSA_FLAG_REGISTERED_IO` 湲곕컲 accept
- ?몄뀡蹂?`RIO_RQ` ?앹꽦
- recv staging buffer ?깅줉 ??`RIOReceive`
- send ??packet buffer ?깅줉 ??`RIOSend`
- `activeSessionCount` 湲곕컲 least-loaded worker 諛곗젙

## 3. 肄붾뱶 援ъ“
### 3-1. server
- [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)
  - backend lifecycle
  - worker/CQ 愿由?  - accept loop
  - send / disconnect / stats

### 3-2. session
- [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)
  - `sessionId`, `slotIndex`, `generation`
  - owner worker index
  - `RIO_RQ`
  - recv staging buffer / recv ring buffer
  - recv pending state
  - send queue ?듦퀎

### 3-3. factory / public layer
- [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FServerFactory.cpp)
  - `Backend: Rio`硫?`FRioServer` ?좏깮
- [IServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\IServer.h)
  - ?곸쐞 ?덉씠?대뒗 backend 醫낅쪟瑜?吏곸젒 紐⑤Ⅸ??

## 4. ?숈옉 ?먮쫫
1. `FRioServer::Start()`媛 Winsock, RIO function table, listen socket, AcceptEx, worker CQ瑜?珥덇린?뷀븳??
2. accept thread媛 `AcceptEx`濡?`WSA_FLAG_REGISTERED_IO` client socket??諛쏅뒗??
3. `AttachAcceptedSocket()`??鍮?slot??癒쇱? 李얘퀬, 洹?slot 湲곗??쇰줈 `FRioSession`, recv registered buffer, `RIO_RQ`瑜??앹꽦?쒕떎.
4. owner worker??`RIO_EVENT_COMPLETION`?쇰줈 源⑥썙吏怨? `RIODequeueCompletion()`?쇰줈 completions瑜??뚮퉬?쒕떎.
5. recv completion? framer / cipher / app handler 寃쎈줈濡??щ씪媛꾨떎.
6. send??packet??framing????temporary registered buffer濡?`RIOSend()`瑜?嫄대떎.

## 5. 以묒슂 ?ㅺ퀎 ?먮떒
### 5-1. CQ ?⑥씪 owner
- CQ瑜??щ윭 ?ㅻ젅?쒓? 怨듭쑀 dequeue?섏? ?딅뒗??
- worker留덈떎 CQ瑜??섎굹 ?먭퀬, ?대떦 worker留??뚮퉬?쒕떎.
- 1李?援ы쁽?먯꽌 ?숆린??蹂듭옟?꾨? ??텛?????좊━?섎떎.

### 5-2. ?몄뀡 owner ?뺤콉
- ?몄뀡? accept ??worker ?섎굹??諛곗젙?쒕떎.
- 諛곗젙 湲곗?? `activeSessionCount` 湲곕컲 least-loaded??
- ?몄뀡 migration? ?대쾲 踰붿쐞?먯꽌 ?섏? ?딅뒗??

### 5-3. lock-free 踰붿쐞
- ?대쾲 baseline? ?쒖젙?뺤꽦 ?곗꽑?앹씠??
- session request queue ?묎렐 ???꾩슂??怨녹뿉??lock???붾떎.
- ?댄썑 hot path 怨꾩륫 ??lock-free ?꾨낫留?遺꾨━?쒕떎.

## 6. ?≫엺 踰꾧렇
### 6-1. 利앹긽
- ?ㅼ쨷 ?몄뀡?먯꽌 `RIOCreateRequestQueue failed. error=10014`媛 諛섎났?먮떎.

### 6-2. ?먯씤
- 珥덇린 援ы쁽? 鍮?slot???뺤젙?섍린 ?꾩뿉 slot ?먯깋 猷⑦봽 ?덉뿉??`RIOCreateRequestQueue()`瑜??몄텧?덈떎.
- slot 0???대? ?ъ슜 以묒씤 寃쎌슦, 媛숈? accepted socket?쇰줈 `RIO_RQ` ?앹꽦???щ윭 踰??쒕룄?????덉뿀??

### 6-3. ?섏젙
- 癒쇱? 鍮?slot??李얜뒗??
- 洹?slot 湲곗??쇰줈 session / registered buffer / `RIO_RQ`瑜???踰덈쭔 ?앹꽦?쒕떎.
- 留덉?留됱뿉 slot attach瑜??섑뻾?쒕떎.

### 6-4. ?섏젙 ?뚯씪
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)

### 6-5. 異붽?濡??≫엺 ?먯썝 遺議?臾몄젣
#### 利앹긽
- `250?몄뀡 / 10遺? 鍮꾧탳 ?곗뿉???쇰? ?몄뀡??`login-response` ?꾩뿉 ?딄꼈??
- ?쒕쾭 濡쒓렇?먮뒗 `RIOCreateRequestQueue failed. error=10055`媛 諛섎났?먮떎.

#### ?먯씤
- baseline 援ы쁽? ?몄뀡??`RIOCreateRequestQueue()` ?덉빟媛믪씠 ?곷??곸쑝濡?而몃떎.
  - ?뱁엳 send reservation??怨쇳븯寃??≫? ?덉뿀??
- ?몄뀡 ???`MaxSessionCount` 湲곗??쇰줈 誘몃━ warm-up?섏? ?딆븘, ?쒖옉 援ш컙 admission 鍮꾩슜????而몃떎.

#### ?섏젙
- `FRioSession::EnsurePoolCapacity(maxSessionCount)`瑜?異붽????몄뀡 ???誘몃━ ?뺣낫?덈떎.
- `FRioServer`??`kMaxOutstandingSend`瑜?`64 -> 8`濡???톬??

#### ?섏젙 ?뚯씪
- [FRioSession.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)
- [FRioSession.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.cpp)
- [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)

## 7. 寃利?寃곌낵
- `Debug x64` ?붾（??鍮뚮뱶 ?깃났
- `RIO 1?몄뀡` ?ㅻえ???깃났
- `RIO 20?몄뀡 / 15珥? ?ㅻえ???깃났
- `RIO 100?몄뀡 / 3遺? ?뚭? ?깃났
- 湲곗〈 `IOCP 100?몄뀡 / 3遺? ?뚭????좎?
- ?섏젙 ??`RIO Direct 250?몄뀡 / 10遺? ?깃났
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\client.log)
  - [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\server.log)
  - [rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\rtt.csv)
  - `echo validation succeeded. sessions=250 responses=619436 ... holdSeconds=600`
  - `RIOCreateRequestQueue failed` 諛쒖깮 `0??

濡쒓렇:
- [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_smoke_20x15s_acceptfix2\server.log)
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_smoke_20x15s_acceptfix2\client.log)
- [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_regression_100x3m\server.log)
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_regression_100x3m\client.log)

## 8. ?꾩옱 寃곕줎
- `RIO`???댁젣 stub???꾨땲???ㅼ젣 baseline backend??
- ?꾩쭅 ?깅뒫 理쒖쟻???④퀎???꾨땲吏留? ?곸쐞 `EchoServer` / `ContentsRuntime` 寃쎈줈瑜??쒖슦???곕뒗 異⑸텇???곹깭??
- `250?몄뀡` 鍮꾧탳 ?곗뿉??蹂댁???`RIOCreateRequestQueue error=10055` admission 臾몄젣???섏젙 ???ы쁽?섏? ?딆븯??
- ?ㅼ쓬 ?묒뾽? `Direct / OwnerThread / IOCP` 鍮꾧탳?, ?꾩슂 ??buffer ?깅줉 鍮꾩슜 理쒖쟻?붾떎.

