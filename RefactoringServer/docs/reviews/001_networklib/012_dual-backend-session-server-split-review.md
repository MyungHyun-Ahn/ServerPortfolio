# Dual Backend Session/Server Split Review

## 1. 紐⑹쟻
- `NetworkLib`媛 `IOCP`? `RIO`瑜?紐⑤몢 backend濡?吏?먰븷 ???덈룄濡??대? 援ъ“瑜?遺꾨━???묒뾽???뺣━?쒕떎.
- ?대쾲 ?④퀎??紐⑺몴??`RIO ?꾩꽦`???꾨땲?? `IOCP ?숈옉 ?좎? + RIO 吏꾩엯???뺣낫`??

## 2. ?듭떖 蹂寃?### 2.1 Session 遺꾨━
- 湲곗〈 ?⑥씪 援ы쁽:
  - `FSession`
- ?꾩옱 援ъ“:
  - [ISession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\ISession.h)
  - [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.h)
  - [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)

`ISession`? 理쒖냼 怨듯넻 ?섎챸二쇨린? ?듦퀎留?媛吏꾨떎.
- `sessionId`
- `slotIndex`
- `generation`
- closing / refcount
- queued send count

IOCP ?꾩슜 ?곹깭??`FIocpSession`?쇰줈 ?대졇??
- `OVERLAPPED`
- `WSABUF`
- recv buffer
- send queue
- `TryBeginSend / EndSend`

RIO ?꾩슜 ?곹깭??`FRioSession`?쇰줈 ???먮━瑜?癒쇱? 留뚮뱾?덈떎.
- ?꾩옱??skeleton留??덇퀬 ?ㅼ젣 `RQ/CQ` ?곹깭???꾩냽 援ы쁽 踰붿쐞??

### 2.2 Server 遺꾨━
- 湲곗〈:
  - [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
- 異붽?:
  - [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)

?꾩옱 ?섎?:
- `FIocpServer`???ㅼ젣 ?숈옉 backend
- `FRioServer`??backend entry stub

### 2.3 Factory ?곌껐
- [FServerFactory.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FServerFactory.cpp)
- `Backend: Iocp` -> `FIocpServer`
- `Backend: Rio` -> `FRioServer`

## 3. IOCP ?뚭? ?щ?
?대쾲 遺꾨━ ?묒뾽?먯꽌 媛??以묒슂??議곌굔? `IOCP ?숈옉 蹂댁〈`?댁뿀??

?뺤씤???댁슜:
- ?붾（??`Debug x64` 鍮뚮뱶 ?깃났
- `EchoServer + EchoClient` 湲곕낯 ?ㅻえ???깃났
- `IOCP 100?몄뀡 / 3遺? ?뚭? ?깃났

寃利?濡쒓렇:
- [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_regression_100x3m\client.log)

寃곌낵:
- `echo validation succeeded. sessions=100 responses=17912 ... holdSeconds=180`
- ?대씪?댁뼵???먮윭 濡쒓렇 鍮꾩뼱 ?덉쓬
- ?쒕쾭 ?먮윭 濡쒓렇 鍮꾩뼱 ?덉쓬

## 4. RIO ?꾩옱 ?곹깭
?꾩옱 `RIO`???쒖꽑??媛?ν븯吏留?誘멸뎄?꾟??곹깭??

?뺤씤???댁슜:
- `Backend: Rio`濡??쒕쾭瑜?湲곕룞?섎㈃ [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)媛 ?좏깮?쒕떎.
- ?꾩옱??紐낆떆?곸쑝濡?`RIO backend is not implemented yet.`瑜?異쒕젰?섍퀬 ?ㅽ뙣 醫낅즺?쒕떎.

寃利?濡쒓렇:
- [rio_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_refactor_smoke\rio_server.log)

???곹깭???μ젏:
- factory, config, public API 寃쎈줈媛 ?대? `RIO`瑜??몄떇?쒕떎.
- ?ㅼ쓬 ?묒뾽? ?대? 援ы쁽留?梨꾩슦硫??쒕떎.

## 5. ?대쾲 ?④퀎?먯꽌 ?섏? ?딆? 寃?- `FRioServer` ?ㅼ젣 send/recv 援ы쁽
- `FRioSession`??RQ/CQ ?곹깭 援ы쁽
- owner-worker ?뺤콉
- session ??湲곕컲 least-loaded 諛곗젙
- CQ ownership ?뺤콉

????ぉ?ㅼ? `RIO` ?ㅼ젣 援ы쁽 ?④퀎?먯꽌 ?ㅼ떆 ?ㅻ，??

## 6. 寃곕줎
- ?대쾲 ?묒뾽?쇰줈 `NetworkLib`??`single IOCP implementation`?먯꽌 `dual-backend濡??뺤옣 媛?ν븳 援ъ“`濡??섏뼱媛붾떎.
- `IOCP` 湲곗??좎? ?좎??먮떎.
- ?댁젣 `RIO`???ㅺ퀎 臾몄꽌留??덈뒗 ?곹깭媛 ?꾨땲?? ?ㅼ젣 肄붾뱶 寃쎄퀎? backend ?좏깮 寃쎈줈瑜?媛吏??곹깭??

