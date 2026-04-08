# Backend Abstraction

## 1. 紐⑹쟻
- `IOCP`, `RIO`, `boost.asio`瑜?媛숈? ?곸쐞 ?쒕쾭 肄붾뱶?먯꽌 鍮꾧탳?????덇쾶 ?쒕떎.
- ?곸쐞 ?좏뵆由ъ??댁뀡? backend 援ы쁽泥대? 吏곸젒 紐⑤Ⅴ怨?`IServer`留??ъ슜?쒕떎.

## 2. ?꾩옱 援ъ“
### 2-1. ?쒕쾭 ?명꽣?섏씠??- `IServer`
  - `Start(const SServerConfig&, IApplicationHandler&)`
  - `Stop()`
  - `Send(uint64_t sessionId, uint16_t opcode, const char* buffer, int32_t length)`
  - `Disconnect(uint64_t sessionId)`
  - `GetBackendKind()`
  - `GetStatsSnapshot()`

### 2-2. ?좏뵆由ъ??댁뀡 ?명꽣?섏씠??- `IApplicationHandler`
  - `OnServerStarted`
  - `OnClientConnected`
  - `OnPacketReceived`
  - `OnClientDisconnected`
  - `OnServerStopped`

### 2-3. ?몄뀡 ?명꽣?섏씠??- `ISession`
  - `sessionId`
  - `slotIndex`
  - `generation`
  - closing / refcount
  - queued send ?듦퀎

## 3. backend 援ы쁽 ?곹깭
- `Iocp`
  - [FIocpServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FIocpSession.h)
  - ?꾩옱 湲곗???backend
- `Rio`
  - [FRioServer](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.h)
  - [FRioSession](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Session\FRioSession.h)
  - `RIO_EVENT_COMPLETION` 湲곕컲 ?쒖닔 RIO backend baseline
- `BoostAsio`
  - ?꾩쭅 stub

backend ?좏깮? [FServerFactory](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FServerFactory.h)媛 ?대떦?쒕떎.

## 4. RIO ?꾩옱 ?뺤콉
- `IOCP + RIO` ?섏씠釉뚮━?쒕뒗 ?대쾲 ?대옒?ㅼ뿉 ?욎? ?딅뒗??
- ?쒖닔 `RIO`??`FRioServer`媛 留〓뒗??
- ?섏씠釉뚮━?쒓? ?꾩슂?섎㈃ ?섏쨷??`FRioIocpServer` 媛숈? 蹂꾨룄 backend濡?遺꾨━?쒕떎.

## 5. ownership ?뺤콉
- worker留덈떎 `RIO_CQ`? owner thread瑜??섎굹 ?붾떎.
- ?몄뀡? accept ??worker ?섎굹??諛곗젙?쒕떎.
- 諛곗젙 湲곗?? `activeSessionCount` 湲곕컲 least-loaded??
- ?몄뀡? disconnect ?꾧퉴吏 owner worker瑜?諛붽씀吏 ?딅뒗??

## 6. ?꾩옱 ?μ젏
- `EchoServer`??backend ??낆쓣 吏곸젒 紐⑤Ⅴ怨??쒖옉?????덈떎.
- `IOCP`? `RIO`瑜?媛숈? config 紐⑤뜽?먯꽌 ?좏깮?????덈떎.
- ?곸쐞 怨꾩링???붾뱾吏 ?딄퀬 transport backend瑜??뺤옣?????덈떎.

## 7. ?꾩옱 ?쒓퀎
- `RIO`??baseline 援ы쁽 ?④퀎??buffer ?깅줉 鍮꾩슜 理쒖쟻?붽? ?꾩쭅 ?녿떎.
- `RIO_IOCP_COMPLETION` 湲곕컲 ?섏씠釉뚮━?쒕뒗 ?꾩쭅 ?녿떎.
- backend蹂??깅뒫 鍮꾧탳????留롮? soak / benchmark媛 ?꾩슂?섎떎.

## 8. ?ㅼ쓬 ?묒뾽
- `RIO` ?μ떆媛?寃利?- `IOCP` / `RIO` 鍮꾧탳 踰ㅼ튂留덊겕
- send/recv hot path 理쒖쟻??- ?꾩슂 ??`FRioIocpServer` ?ㅺ퀎

