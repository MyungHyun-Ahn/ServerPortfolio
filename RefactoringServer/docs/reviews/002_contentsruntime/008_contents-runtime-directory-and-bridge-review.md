# ContentsRuntime ?붾젆?곕━/釉뚮━吏 ?뺣━ 由щ럭

## 1. 紐⑹쟻
- `ContentsRuntime` ?대? 梨낆엫???붾젆?곕━ 湲곗??쇰줈 ??紐낇솗?섍쾶 ?섎늿??
- 肄섑뀗痢?肄붾뱶媛 理쒖냼?쒖쓽 ?몄뀡 ?쒖뼱/議고쉶留??????덈룄濡?`IContentBridge`瑜??뺤옣?쒕떎.

## 2. 諛섏쁺 ?댁슜
### 2.1 ?붾젆?곕━ ?몃텇??- `Core`
  - [ContentRuntimeTypes.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Core\ContentRuntimeTypes.h)
  - [IContent.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Core\IContent.h)
- `Bridge`
  - [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Bridge\IContentBridge.h)
- `Threading`
  - [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.h)
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.cpp)
- `Routing`
  - [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.h)
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)

### 2.2 `IContentBridge` ?뺤옣
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Bridge\IContentBridge.h)
- 異붽???湲곕뒫
  - `DisconnectSession(sessionId)`
  - `IsSessionAlive(sessionId)`
  - `GetCurrentContentId(sessionId)`

### 2.3 ?쒕쾭 ?명꽣?섏씠???뺤옣
- [IServer.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\IServer.h)
- [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.h)
- [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)
- [FStubServer.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FStubServer.h)
- [FStubServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FStubServer.cpp)

## 3. 醫뗭? ??- ?뚯씪 寃쎈줈留?遊먮룄 `?ㅽ뻾 紐⑤뜽`, `?쇱슦??, `怨듭슜 ???, `釉뚮━吏` 梨낆엫??援щ텇?쒕떎.
- `IContentBridge`媛 ?덈Т ?쏀빐???앷린???쒖빟??以꾩씠硫댁꽌?? 肄섑뀗痢좉? `NetworkLib` 援ы쁽 ?몃?瑜?吏곸젒 ???꾩슂???녿떎.
- ?댄썑 `Lobby`, `Room`, disconnect ?뺤콉 媛숈? 肄섑뀗痢좊? 異붽??????ъ궗?⑹꽦??醫뗭븘吏꾨떎.

## 4. 二쇱쓽??- `IContentBridge`瑜??볧옄 ?뚮뒗 怨꾩냽 `?뉗? 釉뚮━吏` ?먯튃???좎??댁빞 ?쒕떎.
- ?뚯폆 ?곹깭, ?몄뀡 ?대? 援ъ“, ???몃? 援ы쁽 媛숈? 寃껋? 怨꾩냽 媛먯텣??
- `DisconnectSession`? ?뺤콉??媛뺥븳 ?숈옉?대?濡? 肄섑뀗痢??꾩씠? 異⑸룎?섏? ?딄쾶 ?ъ슜 ?꾩튂瑜??쒗븳?댁빞 ?쒕떎.

## 5. 寃利?寃곌낵
- `RefactoringServer.sln` x64 Debug ?꾩껜 鍮뚮뱶 ?깃났
- `EchoServer` / `EchoClient` ?ㅻえ???깃났
- ?대씪?댁뼵??濡쒓렇: [contentsruntime_refine_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contentsruntime_refine_client.log)
  - `echo validation succeeded.`
- ?쒕쾭 濡쒓렇: [contentsruntime_refine_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contentsruntime_refine_server.log)
  - `Login -> Chat snapshot -> Echo` ?먮쫫 ?뺤씤

## 6. 寃곕줎
- ?대쾲 ?뺣━??湲곕뒫 異붽?蹂대떎 ?댄썑 ?뺤옣???꾪븳 援ъ“ ?뺣━??媛源앸떎.
- `ContentsRuntime`媛 而ㅼ?湲??꾩뿉 ?붾젆?곕━? 釉뚮━吏 寃쎄퀎瑜??ㅻ벉? ?먮떒? 醫뗫떎.

