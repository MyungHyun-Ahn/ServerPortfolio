# Packet CodeGen And Routing Review

## 1. 臾몄꽌 紐⑹쟻
- ?꾩옱 `PacketGenerator`, `ContentHeader`, generated packet/handler, ?곸쐞 packet router 援ъ“瑜???臾몄꽌?먯꽌 ?뚯븙?????덇쾶 ?뺣━?쒕떎.
- ?⑦궥 ?앹꽦 洹쒖튃怨?肄섑뀗痢좊퀎 泥섎━湲?遺꾨━ 諛⑺뼢?????꾩슂?쒖? 洹쇨굅瑜??④릿??

## 2. ?꾩옱 援ъ“

### 2-1. ?ㅽ궎留?猷⑦듃
- ?⑦궥 ?ㅽ궎留?理쒖긽??猷⑦듃??[Packet](D:\Project\ServerPortfolio\RefactoringServer\Packet)?대떎.
- 肄섑뀗痢좊퀎 ?섏쐞 ?붾젆?곕━濡??섎늿??
- ?꾩옱 ?덉떆:
  - [Echo.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Echo\Echo.yaml)

### 2-2. ?앹꽦湲?- C# ?ㅽ봽?쇱씤 ?앹꽦湲?
  - [PacketGenerator.csproj](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\PacketGenerator.csproj)
  - [Program.cs](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\Program.cs)
- ?섎룞 ?ㅽ뻾 吏꾩엯??
  - [Generate-Packets.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Packets.ps1)
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Packets.cmd)

### 2-3. ?앹꽦 寃곌낵
- generated packet:
  - [EchoPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Packets\Echo\EchoPackets.h)
- 肄섑뀗痢좊퀎 generated handler:
  - [EchoPacketHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Packets\Echo\EchoPacketHandler.h)
- ?곸쐞 generated router:
  - [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Packets\PacketRouter.h)

### 2-4. ?고???怨듯넻 湲곕컲
- 怨듯넻 ?⑦궥 ?명꽣?섏씠??
  - [IContentPacket.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\IContentPacket.h)
- content header:
  - [ContentHeader.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\ContentHeader.h)
- writer / reader:
  - [FPacketWriter.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\FPacketWriter.h)
  - [FPacketReader.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\FPacketReader.h)
- content packet helper:
  - [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\FPacketSerialization.h)

## 3. ?듭떖 ?ㅺ퀎 ?먮떒

### 3-1. `opcode`??transport header媛 ?꾨땲??content header???붾떎
- transport header??湲몄씠, ?뷀샇???? 泥댄겕?ъ쿂???꾩넚 怨꾩링 ?뺣낫留?媛吏꾨떎.
- ?ㅼ젣 硫붿떆吏 ?섎???`opcode`??payload ?좊몢??[ContentHeader.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\ContentHeader.h)濡??대졇??
- ??諛⑺뼢??`NetworkLib`? 肄섑뀗痢?怨꾩링 寃쎄퀎瑜???紐낇솗?섍쾶 留뚮뱺??

### 3-2. generated packet? 怨듯넻 ?명꽣?섏씠?ㅻ? ?곕Ⅸ??- generated packet? 紐⑤몢 [IContentPacket.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\IContentPacket.h)瑜??곕Ⅸ??
- 怨듯넻 怨꾩빟:
  - `GetOpcode()`
  - `Serialize(FPacketWriter&)`
  - `Deserialize(FPacketReader&)`
- payload 硫ㅻ쾭??媛??⑦궥 ?대옒?ㅼ뿉留??붾떎.
  - ?? [EchoPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Packets\Echo\EchoPackets.h)??`message`

### 3-3. `WriteValue` / `ReadValue` free function ???writer/reader 硫ㅻ쾭瑜??대떎
- ?앹꽦 肄붾뱶媛 `writer.Write(...)`, `reader.Read(...)`瑜?吏곸젒 ?몄텧?쒕떎.
- 愿??援ы쁽:
  - [FPacketWriter.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\FPacketWriter.h)
  - [FPacketReader.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\FPacketReader.h)
- ??諛⑺뼢???앹꽦 肄붾뱶 媛?낆꽦怨?IDE ?먯깋??硫댁뿉?????ル떎.

### 3-4. `Rq` / `Rp`??諛섎뱶???띿쑝濡?議댁옱?댁빞 ?쒕떎
- ?앹꽦湲곕뒗 `Rq`留??덇굅??`Rp`留??덉쑝硫??ㅽ뙣?쒕떎.
- `Noti`???좏깮 ?ы빆?대떎.
- ??洹쒖튃? ?앹꽦湲?寃利?濡쒖쭅???ㅼ뼱 ?덈떎:
  - [Program.cs](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\Program.cs)

### 3-5. 媛숈? 移댄뀒怨좊━??泥섎━湲?1媛? ?곸쐞 ?좏깮? router媛 留〓뒗??- `Chat` 媛숈? 肄섑뀗痢?移댄뀒怨좊━ ?꾨옒 硫붿떆吏媛 ?щ윭 媛??덉뼱??泥섎━湲?base??1媛쒕떎.
  - ?? `FChatPacketHandlerBase`
- ????곸쐞 [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Packets\PacketRouter.h)媛 `opcode -> content handler` ?좏깮??留〓뒗??
- ??援ъ“媛 ?쒕え???⑦궥???꾩뿭 泥섎━湲?1媛쒖뿉 紐곗븘?ｋ뒗 諛⑹떇?앸낫???뺤옣?깆씠 醫뗫떎.

## 4. ?꾩옱 Echo ?곸슜 諛⑹떇
- Echo ?쒕쾭??generated router瑜??듯빐 packet??泥섎━?쒕떎:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\Echo\EchoServer\Main.cpp)
- ?먮쫫:
  1. `FIocpServer`媛 transport payload瑜?蹂듯샇??寃利앺븳??
  2. [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\FPacketSerialization.h)?먯꽌 `ContentHeader`瑜??뚯떛?쒕떎.
  3. `packetView.opcode`瑜??삳뒗??
  4. [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Packets\PacketRouter.h)媛 `Echo` handler濡??섍릿??
  5. [EchoPacketHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Packets\Echo\EchoPacketHandler.h)媛 援ъ껜 ?⑦궥 媛앹껜瑜?留뚮뱾怨?`Deserialize` ??`HandleEchoRq` ?깆쓣 ?몄텧?쒕떎.

## 5. ?섎룞 ?앹꽦 ?뺤콉
- ?꾩옱 ?뺤콉? ?쒖뒪?ㅻ쭏媛 諛붾??뚮쭔 ?섎룞 ?앹꽦?앹씠??
- ?댁쑀:
  - C# ?앹꽦湲곕? ??긽 鍮뚮뱶??臾쇰━硫?C++ 諛섎났 鍮뚮뱶媛 ?먮젮吏꾨떎.
  - ?됱냼 C++ ?섏젙留????뚮뒗 ?앹꽦湲곌? ?꾩슂 ?녿떎.
- ?쒖? ?ㅽ뻾:
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Packets.cmd)

## 6. 寃利?洹쇨굅
- ?앹꽦湲??ㅽ뻾 ?깃났:
  - [Generate-Packets.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Packets.cmd)
- generated packet round-trip ?뚯뒪???듦낵:
  - [Main.cpp](D:\Project\ServerPortfolio\RefactoringServer\SmokeTests\LockFreeTests\Main.cpp)
  - [LockFreeTests.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
- Echo ?ㅼ젣 ?뺣났 ?깃났:
  - [EchoServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoServer.exe)
  - [EchoClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\EchoClient.exe)

## 7. ?꾩옱 ?μ젏
- ?ㅽ궎留? generated 肄붾뱶, ?고???helper??寃쎄퀎媛 遺꾨챸?섎떎.
- 肄섑뀗痢좊퀎 泥섎━湲곗? ?곸쐞 ?쇱슦?곌? 遺꾨━???뺤옣???좊━?섎떎.
- ?ν썑 `Chat`, `Login`, `Inventory` 媛숈? 肄섑뀗痢좊? 媛숈? ?⑦꽩?쇰줈 異붽??섍린 ?쎈떎.
- schema type -> C++ / C# type 留ㅽ븨 湲곕컲?대씪 ?섏쨷??C# ?대씪?댁뼵???앹꽦湲??뺤옣 ?ъ?媛 ?덈떎.

## 8. ?⑥? 怨쇱젣
- `Chat`?대굹 `Login` 媛숈? ??踰덉㎏ 肄섑뀗痢좊? 異붽??댁꽌 multi-content router瑜??ㅼ젣濡?寃利앺빐???쒕떎.
- generated output 愿由?洹쒖튃????遺꾨챸?????꾩슂媛 ?덈떎.
  - git 而ㅻ컠 ?ы븿 ?щ?
  - ?ъ깮???쒖젏 洹쒖튃
- ?κ린?곸쑝濡쒕뒗 generated handler ?ъ슜 ?덉떆瑜?`EchoServer/Main.cpp` 諛뽰쓽 ?ㅼ젣 `Contents/<Category>` ?대옒?ㅻ줈 遺꾨━?섎뒗 寃껋씠 醫뗫떎.

## 9. 寃곕줎
- ?꾩옱 援ъ“???쒕떒??Echo ?섑뵆?앹쓣 ?섏뼱???쒖퐯?먯툩蹂??⑦궥 ?앹꽦 + 肄섑뀗痢좊퀎 泥섎━湲?+ ?곸쐞 ?쇱슦?겸앷퉴吏 ?댁뼱吏??1李?怨④꺽?쇰줈 異⑸텇???섎?媛 ?덈떎.
- ?뱁엳 `opcode`瑜?content header濡?遺꾨━?섍퀬, generated router媛 肄섑뀗痢??좏깮??留〓룄濡????먯씠 ?댄썑 MMORPG 肄섑뀗痢??뺤옣???좊━??湲곕컲?대떎.



