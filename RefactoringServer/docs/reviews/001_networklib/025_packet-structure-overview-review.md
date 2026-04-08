# Packet Structure Overview Review

## 1. 臾몄꽌 紐⑹쟻
- `RefactoringServer`???꾩옱 ?⑦궥 援ъ“瑜?肄붾뱶 湲곗??쇰줈 ??踰덉뿉 ?ㅻ챸?섎뒗 臾몄꽌??
- `transport header`, `content header`, generated packet, router, 吏곷젹??洹쒖튃, ?≪닔???먮쫫??寃쎄퀎瑜??뺣━?쒕떎.
- ?댄썑 `PacketGenerator`, `ClientNetworkLib`, `ChattingServer`, `EchoServer`瑜?蹂???怨듯넻 湲곗??먯쑝濡??쇰뒗??

## 2. ?꾩옱 ?⑦궥 怨꾩링

### 2-1. ?꾩껜 諛붿씠??諛곗튂
- ?꾩옱 ?꾩넚 ?⑥쐞???꾨옒 3怨꾩링?쇰줈 ?섎돏??

```text
[ SPacketHeader ][ SContentHeader ][ Content Body ]
```

- `SPacketHeader`
  - ?꾩넚 怨꾩링 ?ㅻ뜑
  - 湲몄씠, randomKey, checksum 媛숈? transport ?뺣낫留?媛吏꾨떎.
- `SContentHeader`
  - 肄섑뀗痢?怨꾩링 ?ㅻ뜑
  - ?꾩옱??`opcode`留?媛吏꾨떎.
- `Content Body`
  - ?ㅼ젣 硫붿떆吏 ?꾨뱶 吏곷젹??寃곌낵??

### 2-2. Transport Header
- ?뺤쓽:
  - [PacketTypes.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Framing\PacketTypes.h)
- ?꾩옱 援ъ“:

```cpp
struct SPacketHeader
{
    std::uint16_t payloadLength;
    std::uint8_t randomKey;
    std::uint8_t checkSum;
};
```

- ?섎?:
  - `payloadLength`
    - `SContentHeader + Content Body` 湲몄씠
  - `randomKey`
    - packet cipher媛 ?덉쓣 ??encode/decode???곕뒗 媛?  - `checkSum`
    - ?꾩옱 payload ?꾩껜?????1諛붿씠??checksum

### 2-3. Content Header
- ?뺤쓽:
  - [ContentHeader.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Framing\ContentHeader.h)
- ?꾩옱 援ъ“:

```cpp
struct SContentHeader
{
    std::uint16_t opcode;
};
```

- ?섎?:
  - transport 怨꾩링? `opcode`瑜?吏곸젒 ?댁꽍?섏? ?딅뒗??
  - `opcode`??payload ?좊몢??`SContentHeader`???ㅼ뼱 ?덈떎.
  - 利?`NetworkLib`??transport framing源뚯?留?梨낆엫吏怨? ?ㅼ젣 硫붿떆吏 醫낅쪟??content 怨꾩링?먯꽌 ?먮떒?쒕떎.

## 3. ?⑦궥 ?뚯쑀沅뚭낵 梨낆엫 寃쎄퀎

### 3-1. NetworkLib媛 梨낆엫吏??寃?- `SPacketHeader` 援ъ꽦
- checksum 怨꾩궛
- optional cipher encode/decode
- recv buffer?먯꽌 transport packet 寃쎄퀎 異붿텧
- `SContentHeader`瑜?遺꾨━??`opcode + body view`濡??섍린湲?
### 3-2. Content / Generated Packet??梨낆엫吏??寃?- `opcode` ?뺤쓽
- body field 吏곷젹????쭅?ы솕
- 硫붿떆吏蹂?handler dispatch

### 3-3. ?듭떖 ?ъ씤??- `opcode`??transport header???덉? ?딅떎.
- `NetworkLib`??湲곕낯?곸쑝濡?`payloadLength / randomKey / checksum + payload`源뚯?留??덈떎.
- ?ㅼ젣 硫붿떆吏 ?섎???`SContentHeader`瑜??뚯떛????generated packet 怨꾩링?먯꽌 ?댁꽍?쒕떎.

## 4. Schema ? Generated Code 援ъ“

### 4-1. Schema ?꾩튂
- ?⑦궥 ?ㅽ궎留덈뒗 [Packet](D:\Project\ServerPortfolio\RefactoringServer\Packet) ?꾨옒 content ?⑥쐞 YAML濡?愿由ы븳??
- ??
  - [Echo.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Echo\Echo.yaml)
  - [Login.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Login\Login.yaml)
  - [Chatting.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Chatting\Chatting.yaml)

### 4-2. Message 醫낅쪟
- ?꾩옱 schema endpoint 醫낅쪟:
  - `rq`
  - `rp`
  - `noti`
  - `broadcast`

- ??
  - [Chatting.yaml](D:\Project\ServerPortfolio\RefactoringServer\Packet\Chatting\Chatting.yaml)
  - `RoomList`??`rq/rp`
  - `Broadcast`??`broadcast`

### 4-3. Generated Output
- ?앹꽦湲?
  - [Program.cs](D:\Project\ServerPortfolio\RefactoringServer\Tools\PacketGenerator\Program.cs)
- ?앹꽦 寃곌낵:
  - packet class:
    - [ChattingPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Chatting\ChattingPackets.h)
  - content handler:
    - [ChattingPacketHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Chatting\ChattingPacketHandler.h)
  - top-level router:
    - [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\PacketRouter.h)

### 4-4. Router ??븷
- ?곸쐞 router??`opcode -> content dispatcher` ?좏깮留??대떦?쒕떎.
- ??
  - [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\PacketRouter.h)
- 媛?content handler base??洹?content ?대? opcode瑜??ㅼ떆 硫붿떆吏蹂꾨줈 遺꾧린?쒕떎.
- ??
  - [ChattingPacketHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Chatting\ChattingPacketHandler.h)

## 5. 吏곷젹??洹쒖튃

### 5-1. 怨듯넻 ?명꽣?섏씠??- 紐⑤뱺 generated packet? [IContentPacket.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\IContentPacket.h)瑜??곕Ⅸ??
- 怨듯넻 怨꾩빟:
  - `GetOpcode()`
  - `ContainsBorrowedViews()`
  - `BindBorrowedViewScope(...)`
  - `GetEstimatedBodySize()`
  - `Serialize(FPacketWriter&)`
  - `Deserialize(FPacketReader&)`

### 5-2. Writer / Reader
- writer:
  - [FPacketWriter.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketWriter.h)
- reader:
  - [FPacketReader.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketReader.h)

### 5-3. ?꾨뱶蹂?洹쒖튃
- scalar
  - 洹몃?濡?`memcpy`?쒕떎.
  - ?꾩옱 ?源껋씠 Windows/x64???ъ떎??little-endian host order 吏곷젹?붾떎.
- `string`, `string_view`
  - `uint32 length + raw bytes`
- `bytes`, `span<const uint8_t>`
  - `uint32 length + raw bytes`
- `vector<T>`
  - `uint32 count + element sequence`
  - scalar vector???곗냽 硫붾え由?bulk copy
- `array<T, N>`
  - 湲몄씠 prefix ?놁쓬
  - ?먯냼 N媛쒕? 怨좎젙 ?쒖꽌?濡?湲곕줉
- `map<K, V>`, `unordered_map<K, V>`
  - `uint32 count + key/value pair sequence`

### 5-4. Borrowed View
- 紐⑤뱺 ??쭅?ы솕媛 ?뚯쑀??蹂듭궗留??곕뒗 嫄??꾨땲??
- ?덈? ?ㅼ뼱 [EchoPackets.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Echo\EchoPackets.h) ??`string_view` ?꾨뱶??recv buffer 硫붾え由щ? 吏곸젒 媛由ы궗 ???덈떎.
- ??寃쎌슦:
  - `ContainsBorrowedViews() == true`
  - `BindBorrowedViewScope(...)` 濡??섎챸 scope瑜?臾띕뒗??
- 諛섎?濡?`Chatting`??`bytes -> vector<uint8_t>` 媛숈? ?뚯쑀???꾨뱶????쭅?ы솕 ??蹂듭궗蹂몄쓣 媛吏꾨떎.

## 6. Send 寃쎈줈

### 6-1. Content Packet ?앹꽦
- helper:
  - [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketSerialization.h)
- ?먮쫫:
  1. generated packet??body瑜?`FPacketWriter`??`Serialize`
  2. writer front??`SContentHeader` 怨듦컙??誘몃━ reserve
  3. `BuildOutgoingContentPacket(...)` 媛 `opcode`瑜?`SContentHeader`濡?梨꾩?
  4. 寃곌낵??`FOutgoingContentPacket`

### 6-2. 以묒슂????- `FOutgoingContentPacket`??payload???대?

```text
[ SContentHeader ][ Content Body ]
```

?뺥깭??

### 6-3. ?쒕쾭 ?꾩넚 吏곸쟾
- IOCP:
  - [FIocpServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FIocpServer.cpp)
- RIO:
  - [FRioServer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Servers\Core\FRioServer.cpp)

- ?ㅼ젣 ?쒖꽌:
  1. `FOutgoingContentPacket`?먯꽌 payload buffer ?띾뱷
  2. cipher媛 ?덉쑝硫?payload ?꾩껜(`SContentHeader + Body`)瑜?encode
  3. encode??payload 湲곗??쇰줈 checksum 怨꾩궛
  4. `SPacketHeader` ?앹꽦
  5. 理쒖쥌?곸쑝濡?
```text
[ SPacketHeader ][ encoded payload ]
```

?뺥깭濡??꾩넚

## 7. Receive 寃쎈줈

### 7-1. Transport Packet 異붿텧
- framer:
  - [FDefaultPacketFramer.cpp](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Framing\FDefaultPacketFramer.cpp)
- `TryExtractPacketView(...)` ??recv buffer?먯꽌:
  - `SPacketHeader`
  - payload pointer
  - payloadLength
瑜??쎌뼱??`FPacketView`瑜?留뚮뱺??

### 7-2. 1李?PacketView
- ?뺤쓽:
  - [FPacketView.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\View\FPacketView.h)
- ???쒖젏??`FPacketView`??
  - `randomKey`
  - `checkSum`
  - payload pointer
  - payloadLength
瑜?媛뽰?留?
  - `opcode`???꾩쭅 0?대떎.

### 7-3. 寃利앷낵 decode ?쒖꽌
- ?꾩옱 IOCP/RIO recv 怨듯넻 ?쒖꽌??
  1. framer媛 transport payload view 異붿텧
  2. payload checksum 寃利?  3. cipher媛 ?덉쑝硫?payload decode
  4. `TryParseContentPacketView(...)` 濡?`SContentHeader` ?뚯떛
  5. 理쒖쥌 `opcode + body payload` view ?앹꽦

- helper:
  - [FPacketSerialization.h](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Packet\Serialization\FPacketSerialization.h)

### 7-4. 理쒖쥌 Content PacketView
- `TryParseContentPacketView(...)` ?댄썑??`FPacketView`??
  - `opcode`
  - body payload pointer
  - body payloadLength
瑜?媛吏꾨떎.
- 利????쒖젏遺?곕뒗 `SContentHeader`媛 ?쒓굅??content body view??

## 8. Dispatch 寃쎈줈

### 8-1. Top-level Router
- [PacketRouter.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\PacketRouter.h)
- ??븷:
  - `opcode`瑜?蹂닿퀬 ?대뼡 content dispatcher?먭쾶 ?섍만吏 寃곗젙

### 8-2. Content Handler Base
- ??
  - [ChattingPacketHandler.h](D:\Project\ServerPortfolio\RefactoringServer\Generated\Packets\Chatting\ChattingPacketHandler.h)
- ??븷:
  1. `opcode` switch
  2. ?대떦 packet type ?앹꽦
  3. `DeserializeContentPacket(packetView, packet)`
  4. `HandleXxx(...)` ?몄텧

### 8-3. ?꾩옱 援ъ“???μ젏
- transport 怨꾩링? 硫붿떆吏 醫낅쪟瑜?紐곕씪???쒕떎.
- content 異붽? ??YAML怨?generated code留??섎㈃ ?쒕떎.
- router媛 content group ?⑥쐞 遺꾧린瑜?留≪븘, handler 援ъ“媛 鍮꾧탳???⑥닚?섎떎.

## 9. ?꾩옱 援ъ“瑜?蹂???二쇱쓽????
### 9-1. Transport checksum? body留뚯씠 ?꾨땲??content payload ?꾩껜 湲곗??대떎
- ?꾩옱 checksum ??곸? `Content Body`留뚯씠 ?꾨땲??
```text
[ SContentHeader ][ Content Body ]
```

?꾩껜??

### 9-2. opcode??wire??留??욎씠 ?꾨땲??- wire 留??욎? ??긽 `SPacketHeader`??
- `opcode`??payload ?대? 泥?2諛붿씠?몄씤 `SContentHeader`???덈떎.

### 9-3. 吏곷젹?붾뒗 ?ㅽ듃?뚰겕 ?쒖? endian 蹂?섏쓣 ?곕줈 ?섏? ?딅뒗??- ?꾩옱 scalar 吏곷젹?붾뒗 host memory layout??洹몃?濡??대떎.
- 吏湲??源??섍꼍?먯꽌???ъ떎??Windows little-endian ?꾩젣??
- 異뷀썑 ?댁떇?깆쓣 ?볧엳?ㅻ㈃ endian policy瑜?蹂꾨룄濡??≪븘???쒕떎.

### 9-4. packet 醫낅쪟???곕씪 ??쭅?ы솕 鍮꾩슜???ㅻⅤ??- `string_view`, `span` 湲곕컲 packet? zero-copy ?깃꺽??媛吏꾨떎.
- `std::string`, `std::vector<uint8_t>` 湲곕컲 packet? ?뚯쑀??蹂듭궗媛 諛쒖깮?쒕떎.

## 10. ?뺣━
- ?꾩옱 `RefactoringServer` ?⑦궥 援ъ“???듭떖?
  - transport header? content header瑜?遺꾨━?섍퀬
  - content schema瑜?YAML濡?愿由ы븯硫?  - generated packet/handler/router媛 `opcode` 湲곕컲 遺꾧린瑜??대떦?섎뒗 援ъ“??
- 諛붿씠??湲곗??쇰줈??
```text
[ SPacketHeader ][ SContentHeader ][ Content Body ]
```

媛 湲곕낯 ?뺤떇?대떎.
- `NetworkLib`??framing/cipher/checksum/recv view源뚯? 留↔퀬,
- generated packet 怨꾩링? `opcode` ?댁꽍怨?field 吏곷젹????쭅?ы솕瑜?留〓뒗??
- ?댄썑 ?덈줈??肄섑뀗痢좊? 異붽????뚮룄 ??寃쎄퀎瑜??좎??섎뒗 寃껋씠 ?꾩옱 援ъ“? 媛????留욌뒗??

