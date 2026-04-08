# Dual Backend RIO Support Plan

## 1. 紐⑹쟻
- `NetworkLib`媛 `IOCP`? `RIO`瑜?媛숈? ?곸쐞 API?먯꽌 蹂묓뻾 吏?먰븯?꾨줉 ?뺤옣?쒕떎.
- `EchoServer`, `ContentsRuntime`, packet handler??transport 援ы쁽泥대? 吏곸젒 紐⑤Ⅴ怨?`IServer`留??ъ슜?쒕떎.
- `IOCP + RIO` ?섏씠釉뚮━?쒕뒗 ?대쾲 ?④퀎?먯꽌 ?ㅻ（吏 ?딄퀬, ?섏쨷??蹂꾨룄 backend(`FRioIocpServer`)濡?遺꾨━?쒕떎.

## 2. ?꾩옱 寃곕줎
- 諛⑺뼢? `IOCP 援먯껜`媛 ?꾨땲??`IOCP ?좎? + ?쒖닔 RIO 異붽?`??
- ?꾩옱 `RIO`??`RIO_EVENT_COMPLETION` 湲곕컲???쒖닔 RIO backend濡?援ы쁽?쒕떎.
- `RIO_IOCP_COMPLETION` 湲곕컲 ?섏씠釉뚮━?쒕뒗 媛숈? ?대옒?ㅼ뿉 ?욎? ?딄퀬 ?꾩냽 backend濡?遺꾨━?쒕떎.

## 3. ?꾩옱 援ъ“
### 3-1. Public Layer
- [IServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\IServer.h)
- [IApplicationHandler](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\IApplicationHandler.h)

### 3-2. Backend Layer
- [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
- [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)
- [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FServerFactory.h)

### 3-3. Session Layer
- [ISession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\ISession.h)
- [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.h)
- [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)

## 4. ?쒖닔 RIO 1李?援ы쁽 踰붿쐞
- `FRioServer` startup / shutdown 援ы쁽
- RIO function table 濡쒕뱶
- worker蹂?`RIO_CQ + event + owner thread` ?앹꽦
- `AcceptEx + WSA_FLAG_REGISTERED_IO` 湲곕컲 accept 寃쎈줈
- ?몄뀡蹂?`RIO_RQ` ?앹꽦
- recv staging buffer ?깅줉怨?`RIOReceive` post
- send ??packet buffer ?깅줉 ??`RIOSend`
- `RIO_EVENT_COMPLETION` 湲곕컲 CQ notification / dequeue
- `session -> owner worker` 諛곗젙 ?뺤콉
  - ?꾩옱 ?뺤콉? `activeSessionCount` 湲곕컲 least-loaded

## 5. 援ы쁽 ?뺤콉
### 5-1. CQ ownership
- 怨듭쑀 CQ瑜??щ윭 ?ㅻ젅?쒓? 媛숈씠 ?뚮퉬?섏? ?딅뒗??
- worker留덈떎 CQ瑜??섎굹 ?먭퀬, ?대떦 CQ??owner worker thread留?dequeue?쒕떎.

### 5-2. session ownership
- ?몄뀡? accept ??worker ?섎굹??諛곗젙?쒕떎.
- 諛곗젙 湲곗?? `activeSessionCount`媛 媛???곸? worker??
- ?몄뀡? disconnect ?꾧퉴吏 owner worker瑜?諛붽씀吏 ?딅뒗??

### 5-3. ?숆린???뺤콉
- 1李?援ы쁽? ?뺥솗???곗꽑?대떎.
- session request queue ?묎렐泥섎읆 ?꾩슂??怨녹뿉??lock???덉슜?쒕떎.
- ?댄썑 蹂묐ぉ???뺤씤??hot path留?lock-free ?먮뒗 near lock-free濡??꾪솚?쒕떎.

## 6. 援ы쁽 以??뺤씤???듭떖 ?댁뒋
- ?ㅼ쨷 ?몄뀡 珥덇린 援ы쁽?먯꽌 `RIOCreateRequestQueue failed. error=10014`媛 諛쒖깮?덈떎.
- ?먯씤? 鍮?session slot???뺤젙?섍린 ?꾩뿉 媛숈? accepted socket?쇰줈 `RIOCreateRequestQueue`瑜?諛섎났 ?쒕룄?????덉뿀??援ъ“???
- ?섏젙 ?꾩뿉??  - 癒쇱? 鍮?slot??李얘퀬
  - 洹?slot 湲곗??쇰줈 session / registered buffer / request queue瑜???踰덈쭔 ?앹꽦
  - 留덉?留됱뿉 slot??attach
  ?쒖꽌濡?諛붽엥??

## 7. ?꾩옱 寃利?寃곌낵
- `Debug x64` ?붾（??鍮뚮뱶 ?깃났
- `IOCP 100?몄뀡 / 3遺? ?뚭? ?깃났
- `RIO 1?몄뀡` ?ㅻえ???깃났
- `RIO 20?몄뀡 / 15珥? ?ㅻえ???깃났
- `RIO 100?몄뀡 / 3遺? ?뚭? ?깃났

寃利?濡쒓렇 ?덉떆:
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_smoke_20x15s_acceptfix2\client.log)
- [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_smoke_20x15s_acceptfix2\server.log)
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_regression_100x3m\client.log)

## 8. ?ㅼ쓬 ?④퀎
1. `RIO` ?μ떆媛?soak怨??깅뒫 痢≪젙
2. `IOCP` / `RIO` 鍮꾧탳 踰ㅼ튂留덊겕
3. send/recv buffer ?깅줉 鍮꾩슜 理쒖쟻??4. hot path lock-free ?꾨낫 援ш컙 怨꾩륫
5. ?꾩냽 backend濡?`FRioIocpServer` ?ㅺ퀎

