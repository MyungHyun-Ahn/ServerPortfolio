# Packet Framer Review

## 1. 臾몄꽌 紐⑹쟻
- `NetworkLib` ?⑦궥 ?꾨젅?대컢 怨꾩링???꾩옱 ?대뼡 ?ㅻ뜑 援ъ“瑜??ъ슜?섍퀬 ?덈뒗吏 ?뺣━?쒕떎.
- ?덇굅???꾨줈?앺듃???⑦궥 援ъ“?먯꽌 臾댁뾿??怨꾩듅?덇퀬 臾댁뾿??踰꾨졇?붿? 洹쇨굅瑜??④릿??
- `FIocpServer` ?≪닔??寃쎄퀎???꾨젅?대㉧? cipher瑜??곌껐??寃곌낵瑜?寃?좏븳??

## 2. ?꾩옱 援ы쁽 踰붿쐞
- ?⑦궥 ????뺤쓽:
  - [PacketTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\PacketTypes.h)
- ?꾨젅?대㉧ ?명꽣?섏씠??
  - [IPacketFramer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\IPacketFramer.h)
- 湲곕낯 援ы쁽:
  - [FDefaultPacketFramer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.h)
  - [FDefaultPacketFramer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Packet\FDefaultPacketFramer.cpp)
- 肄붿뼱 ?곌껐:
  - [BackendTypes.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\BackendTypes.h)
  - [IServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
  - [IApplicationHandler.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)
  - [FIocpServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.h)
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FIocpServer.cpp)

## 3. ?ㅻ뜑 ?ㅺ퀎 洹쇨굅

### 3-1. ?덇굅??援ъ“?먯꽌 怨꾩듅??寃?- `payloadLength`
  - TCP??硫붿떆吏 寃쎄퀎瑜?蹂댁옣?섏? ?딆쑝誘濡??꾨젅??湲몄씠???ъ쟾???꾩슂?섎떎.
- `randomKey`
  - 湲곕낯 ?⑦궥 ?뷀샇???낅젰 媛믪쑝濡?怨꾩냽 ?ъ슜?쒕떎.
- `checkSum`
  - ?섏떊 ??payload 臾닿껐?깆쓣 媛蹂띻쾶 ?뺤씤?섎뒗 理쒖냼 寃利??섎떒?쇰줈 ?좎??덈떎.

### 3-2. ?덇굅??援ъ“?먯꽌 諛붽씔 寃?- ?꾩뿭 `PACKET_CODE`
  - ??援ъ“?먯꽌???꾩뿭 ?ㅼ젙 ?섏〈???쒓굅?덈떎.
- payload ?좊몢 `WORD type`
  - ?묒슜 怨꾩링 洹쒖빟??payload ?대? 愿濡???⑥뼱 ?덉쑝硫?肄붿뼱? ?곸쐞 濡쒖쭅 寃쎄퀎媛 ?먮젮吏꾨떎.
  - ??援ъ“?먯꽌??`opcode`瑜??ㅻ뜑濡??밴꺽?덈떎.

### 3-3. Packet Header V1
- ?꾩옱 ?ㅻ뜑 ?꾨뱶???꾨옒? 媛숇떎.
  - `opcode`
  - `payloadLength`
  - `randomKey`
  - `checkSum`

## 4. 泥섎━ ?쒖꽌

### 4-1. Send 寃쎈줈
1. ?묒슜 怨꾩링??`opcode`? plaintext payload瑜??꾨떖?쒕떎.
2. cipher媛 ?덉쑝硫?payload瑜??뷀샇?뷀븳??
3. ?뷀샇?붾맂 payload 湲곗??쇰줈 `checkSum`??怨꾩궛?쒕떎.
4. ?꾨젅?대㉧媛 ?ㅻ뜑瑜?遺숈뿬 ?꾩넚 踰꾪띁瑜?留뚮뱺??
5. `WSASend`濡??꾩넚?쒕떎.

### 4-2. Recv 寃쎈줈
1. ?몄뀡 ?꾩쟻 踰꾪띁???섏떊 諛붿씠?몃? append ?쒕떎.
2. ?꾨젅?대㉧媛 ?⑦궥 ?섎굹瑜?異붿텧?쒕떎.
3. 異붿텧??payload 湲곗??쇰줈 `checkSum`??寃利앺븳??
4. cipher媛 ?덉쑝硫?payload瑜?蹂듯샇?뷀븳??
5. `opcode`? plaintext payload瑜??묒슜 怨꾩링???꾨떖?쒕떎.

## 5. ?명꽣?섏씠??蹂寃??댁쑀
- [IServer.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IServer.h)
  - `Send()`??`opcode`瑜?異붽??덈떎.
- [IApplicationHandler.h](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\IApplicationHandler.h)
  - `OnPacketReceived()`媛 `opcode`瑜?吏곸젒 諛쏅룄濡?諛붽엥??

??蹂寃??뺣텇???묒슜 怨꾩링? ???댁긽 payload 泥?諛붿씠?몃굹 泥?`WORD`瑜?吏곸젒 ?댁꽍?섏? ?딆븘???쒕떎.

## 6. ?곸슜 寃곌낵
- [EchoServer/Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\Main.cpp)
  - ?붿껌 `opcode`瑜?蹂닿퀬 ?묐떟 `opcode`瑜??좏깮?쒕떎.
- [EchoClient/Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\Main.cpp)
  - ?붿껌 ?⑦궥 ?앹꽦 ??`opcode`, `checkSum`???ы븿?쒕떎.
  - ?묐떟 ?섏떊 ??`opcode`, `checkSum`??寃利앺븳??
  - ?ㅼ쨷 ?붿껌, 遺꾪븷 ?≪떊, ?꾩쟻 ?섏떊???뚯뒪???몄옄濡??쒖뼱?????덈떎.

## 7. 寃利?洹쇨굅
- 鍮뚮뱶 ?깃났:
  - [NetworkLib.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\NetworkLib.vcxproj)
  - [LockFreeTests.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\LockFreeTests.vcxproj)
  - [EchoServer.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoServer\EchoServer.vcxproj)
  - [EchoClient.vcxproj](D:\Project\ServerPortfolio\RefactoringServer\EchoClient\EchoClient.vcxproj)
- ?뚯뒪???깃났:
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
    - `Packet framer round trip`
    - `Packet framer partial receive`
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)
    - `response: echo-test`
    - `echo validation succeeded.`
    - `--count 8 --payload-size 48 --send-chunk-size 5 --send-chunk-delay-ms 1 --recv-buffer-size 11`
      - ?ㅼ쨷 ?⑦궥 ?뺣났 ?깃났
      - 遺꾪븷 ?≪떊 ?깃났
      - ?묒? recv buffer 湲곕컲 ?꾩쟻 ?섏떊 ?깃났

## 8. ?꾩옱 ?쒓퀎
- ?ㅻ뜑??`sequence`, `version`, `magic`? ?꾩쭅 ?녿떎.
- `EchoClient`???댁젣 ?꾩쟻 ?섏떊??吏?먰븯吏留? ?섎룄?곸쑝濡??묐떟 ?쒖꽌瑜??ㅼ꽎???쒕쾭 ?쒕굹由ъ삤源뚯? 寃利앺븯??援ъ“???꾩쭅 ?꾨땲??

## 9. 寃곕줎
- ??`NetworkLib`???덇굅???⑦궥 援ъ“瑜?洹몃?濡?蹂듭젣?섏? ?딆븯??
- ????덇굅?쒖뿉???좏슚?덈뜕 湲몄씠, ?쒖닔 ?? 泥댄겕??媛쒕뀗? 怨꾩듅?덈떎.
- 洹몃━怨?payload ?좊몢 ???愿濡瑜??놁븷怨?`opcode`瑜??ㅻ뜑濡??밴꺽???묒슜 怨꾩링怨??꾩넚 怨꾩링 寃쎄퀎瑜???紐낇솗??留뚮뱾?덈떎.
