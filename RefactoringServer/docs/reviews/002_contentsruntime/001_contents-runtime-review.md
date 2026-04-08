# ContentsRuntime 由щ럭

## 1. 紐⑹쟻
- ?덇굅???꾨줈?앺듃??`肄섑뀗痢??꾩슜 ?ㅻ젅??+ ?꾨젅??湲곕컲 泥섎━` ?⑦꽩??媛?몄삤?? `NetworkLib`???寃고빀? ??텛??諛⑺뼢?쇰줈 ?ш뎄?깊뻽??
- 肄섑뀗痢??ㅻ젅???먯껜瑜?`NetworkLib` ?덉뿉 ?ｌ? ?딄퀬 蹂꾨룄 ?꾨줈?앺듃 `ContentsRuntime`濡?遺꾨━??梨낆엫 寃쎄퀎瑜?紐낇솗???덈떎.
- 1李?紐⑺몴??`Login -> AuthContent -> EchoContent` ?먮쫫???ㅼ젣濡??숈옉?섎뒗吏 寃利앺븯??寃껋씠?덈떎.

## 2. ?대쾲 ?묒뾽?먯꽌 異붽???援ъ“
### 2.1 ?꾨줈?앺듃 遺꾨━
- ???꾨줈?앺듃: [ContentsRuntime.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\ContentsRuntime.vcxproj)
- `NetworkLib`???ㅽ듃?뚰겕 I/O? ?몄뀡 ?섎챸二쇨린留??대떦?쒕떎.
- `ContentsRuntime`??肄섑뀗痢??ㅻ젅?? 肄섑뀗痢??쇱슦?? 肄섑뀗痢?媛??대룞???대떦?쒕떎.

### 2.2 ?듭떖 援ъ꽦
- [IContent.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Core\IContent.h)
  - `OnEnter`, `OnLeave`, `OnPacket`, `OnFrame`瑜??쒓났?섎뒗 肄섑뀗痢?理쒖냼 ?명꽣?섏씠??- [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.h)
  - 肄섑뀗痢??섎굹瑜??대떦?섎뒗 ?꾩슜 ?ㅻ젅??  - `enter`, `leave`, `packet` ?먮? 泥섎━?쒕떎
- [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.h)
  - 肄섑뀗痢??깅줉, ?ㅻ젅???쒖옉/醫낅즺, `sessionId -> contentId` 留ㅽ븨, 釉뚮━吏 援ы쁽 ?대떦
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Bridge\IContentBridge.h)
  - `SendRaw`, `MoveSession`, `DisconnectSession`, `IsSessionAlive`, `GetCurrentContentId` ?쒓났

## 3. 援ъ“媛 醫뗭? ??### 3.1 NetworkLib? 肄섑뀗痢??ㅽ뻾 紐⑤뜽 遺꾨━
- `NetworkLib`??`sessionId + opcode + payload`源뚯? ?섍린怨??앸궃??
- 肄섑뀗痢??ㅻ젅??紐⑤뜽? `ContentsRuntime`媛 ?곕줈 ?뚯쑀?섎?濡??ㅽ듃?뚰겕 肄붿뼱 梨낆엫??遺덉뼱?섏? ?딅뒗??

### 3.2 ?덇굅???⑦꽩 怨꾩듅
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Threading\FContentThread.cpp)?먯꽌
  - `enterQueue`
  - `leaveQueue`
  - `packetQueue`
  瑜??쒖꽌?濡?泥섎━?쒕떎.
- ?꾨젅??二쇨린??`GetTargetFps()` 湲곗??쇰줈 ?뚭린 ?뚮Ц???덇굅?쒖쓽 `ContentsThread + FrameTask` ?댁쁺 ?⑦꽩???먯뿰?ㅻ읇寃?怨꾩듅?덈떎.

### 3.3 肄섑뀗痢?寃쎄퀎?먯꽌 owned payload ?ъ슜
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\ContentsRuntime\Routing\FContentRuntime.cpp)??`EnqueuePacket(...)`??payload瑜?`FOwnedPacketEnvelope`濡?留뚮뱾??肄섑뀗痢??먯뿉 ?ｋ뒗??
- ???④퀎?먯꽌 `string_view`, `bytes_view` 媛숈? borrowed payload瑜?吏곸젒 ?섍린吏 ?딄린 ?뚮Ц??肄섑뀗痢??ㅻ젅??寃쎄퀎?먯꽌 ?섎챸 臾몄젣媛 以꾩뼱?좊떎.

## 4. ?꾩옱 ?섑뵆 ?곸슜
- [FAuthContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Auth\FAuthContent.cpp)
  - `LoginRq`瑜?諛쏄퀬 `LoginRp`瑜?蹂대궦 ???깃났 ??`MoveSession`?쇰줈 `EchoContent`濡??대룞
- [FEchoContent.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Contents\Echo\FEchoContent.cpp)
  - `EchoRq`
  - `RoomSnapshotRq`
  泥섎━
- [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
  - `OnClientConnected`?먯꽌 `AuthContent` 吏꾩엯
  - `OnPacketReceived`?먯꽌 `FContentRuntime.EnqueuePacket(...)`留??몄텧
  - `OnClientDisconnected`?먯꽌 `LeaveSession(...)`

## 5. 寃利??곹깭
- `RefactoringServer.sln` x64 Debug 鍮뚮뱶 ?깃났
- `EchoServer` + `EchoClient` 理쒖냼 ?ㅻえ???깃났
  - `Login -> Chat snapshot -> Echo`
  - `echo validation succeeded.` ?뺤씤
- ?쒕쾭 濡쒓렇?먯꽌
  - `auth content enter`
  - `login succeeded`
  - `auth content leave`
  - `echo content enter`
  ?먮쫫 ?뺤씤

## 6. ?꾩옱 由ъ뒪??### 6.1 吏??諛섎났 ?쒕굹由ъ삤
- ?⑤컻 寃쎈줈??寃利앺뻽吏留? 諛섎났/?μ떆媛??쒕굹由ъ삤??蹂꾨룄 寃利앹씠 ?꾩슂?덈떎.
- ?댄썑 lock-free inbox, race injection, 2?쒓컙 寃利??뚮옖???ш린???댁뼱議뚮떎.

### 6.2 肄섑뀗痢???援ъ“
- 珥덇린 援ы쁽? `std::mutex + std::condition_variable + std::deque` 湲곕컲?댁뿀??
- 援ъ“ 寃利앹뿉??異⑸텇?섏?留? ?댄썑 遺?섏뿉??寃쏀빀??而ㅼ쭏 媛?μ꽦???덉뼱 hot path 痢≪젙怨?寃쎈웾?붽? ?꾩냽 ?묒뾽?쇰줈 ?댁뼱議뚮떎.

## 7. 寃곕줎
- `ContentsRuntime`瑜?蹂꾨룄 ?꾨줈?앺듃濡?遺꾨━???먮떒? 醫뗫떎.
- ?덇굅?쒖쓽 醫뗭? ?ㅽ뻾 ?⑦꽩? ?대━怨? `NetworkLib`???媛뺥븳 寃고빀? ?쇳븯??援ъ“媛 ?먮떎.
- ?댄썑 怨쇱젣???덉젙??寃利앷낵 肄섑뀗痢??꾩씠 洹쒖튃 ?뺤갑, 洹몃━怨?inbox 寃쎈웾?붾떎.



