# PacketGenerator

媛꾨떒??YAML ?⑦궥 ?ㅽ궎留덈? ?쎌뼱??C++ ?⑦궥/?몃뱾???쇱슦???ㅻ뜑瑜??앹꽦?섎뒗 ?ㅽ봽?쇱씤 ?꾧뎄?낅땲??

## ??븷
- `RefactoringServer/Packet/**/*.yaml` ?ㅽ궎留덈? ?쎈뒗??
- `RefactoringServer/Generated/Cpp/Packets/**` ?꾨옒??generated C++ ?ㅻ뜑瑜?留뚮뱺??
- 肄섑뀗痢좊퀎 packet/handler? ?꾩뿭 `PacketRouter.h`瑜??앹꽦?쒕떎.

## 湲곕낯 ?ㅽ뻾

?꾨줈?앺듃 猷⑦듃?먯꽌:

```powershell
RefactoringServer\scripts\generate\Generate-Packets.cmd
```

吏곸젒 ?ㅽ뻾????

```powershell
dotnet run --project RefactoringServer\Tools\PacketGenerator\PacketGenerator.csproj
```

## ?몄옄
- `--schema-root <path>`
  - 湲곕낯媛? `RefactoringServer/Packet`
- `--output-root <path>`
  - 湲곕낯媛? `RefactoringServer/Generated/Cpp/Packets`
- `--csharp-output-root <path>`
  - 湲곕낯媛? `RefactoringServer/Generated/CSharp/Packets`
- `--targets <list>`
  - `cpp`
  - `csharp`
  - `cpp,csharp`

??

```powershell
dotnet run --project RefactoringServer\Tools\PacketGenerator\PacketGenerator.csproj -- --schema-root D:\Project\ServerPortfolio\RefactoringServer\Packet --output-root D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Packets --csharp-output-root D:\Project\ServerPortfolio\RefactoringServer\Generated\CSharp\Packets --targets cpp,csharp
```

## ?ㅽ궎留?洹쒖튃
- ?뚯씪 1媛쒓? 肄섑뀗痢?1媛쒕떎.
- ??
  - `Packet/Echo/Echo.yaml`
  - `Packet/Login/Login.yaml`
  - `Packet/Chat/Chat.yaml`
- ???뚯씪 ?덉뿉 ?щ윭 `messages`瑜??????덈떎.
- `rq`? `rp`??諛섎뱶???띿쑝濡?議댁옱?댁빞 ?쒕떎.
- `noti`???좏깮 ?ы빆?대떎.
- `rq`, `rp`, `noti`??媛곴컖 ?ㅻⅨ `opcode`瑜?媛?몄빞 ?쒕떎.
- ?꾩껜 ?ㅽ궎留?吏묓빀?먯꽌 `content` ?대쫫怨?`opcode`??以묐났?섎㈃ ???쒕떎.

## 吏?????
- scalar
  - `bool`
  - `int8`, `int16`, `int32`, `int64`
  - `uint8`, `uint16`, `uint32`, `uint64`
  - `float`
  - `double`
- special
  - `string`
  - `bytes`
- container
  - `vector<T>`
  - `array<T, N>`
  - `map<K, V>`
  - `unordered_map<K, V>`

## 而⑦뀒?대꼫 ?뺤콉
- 怨듭떇 吏?먯? `而⑦뀒?대꼫 1?④퀎`源뚯?留뚯씠??
- ?덉슜 ??
  - `vector<string>`
  - `map<string, uint32>`
  - `unordered_map<string, string>`
- 湲덉? ??
  - `vector<vector<int32>>`
  - `map<string, vector<uint32>>`
  - `unordered_map<string, map<string, int32>>`

蹂듭옟??nested container媛 ?꾩슂?섎㈃ ?앹꽦湲?湲곕낯 洹쒖튃???뺤옣?섍린蹂대떎, ?앹꽦???⑦궥?먯꽌 `Serialize` / `Deserialize`瑜??섎룞 override?섎뒗 履쎌쓣 ?곗꽑 沅뚯옣?⑸땲??

## ?앹꽦 寃곌낵
- 肄섑뀗痢좊퀎:
  - `<Content>Packets.h`
  - `<Content>PacketHandler.h`
- C# 肄섑뀗痢좊퀎:
  - `<Content>Packets.g.cs`
- ?꾩뿭:
  - `PacketRouter.h`
  - `GeneratedPacketRegistry.g.cs`

??
- `Generated/Cpp/Packets/Echo/EchoPackets.h`
- `Generated/Cpp/Packets/Echo/EchoPacketHandler.h`
- `Generated/Cpp/Packets/PacketRouter.h`
- `Generated/CSharp/Packets/Echo/EchoPackets.g.cs`
- `Generated/CSharp/Packets/GeneratedPacketRegistry.g.cs`

## ?앹꽦 肄붾뱶 援ъ“
- packet class??`IContentPacket`???곕Ⅸ??
- 湲곕낯?곸쑝濡??꾨옒瑜??앹꽦?쒕떎.
  - `GetOpcode()`
  - `virtual Serialize(FPacketWriter&) const`
  - `virtual Deserialize(FPacketReader&)`
- 肄섑뀗痢좊퀎 handler base / dispatcher??媛숈씠 ?앹꽦?쒕떎.
- ?꾩뿭 router媛 `opcode`濡??대뼡 肄섑뀗痢?泥섎━湲곗뿉 ?섍만吏 ?좏깮?쒕떎.

## ?ㅽ뙣 議곌굔
- `rq`留??덇퀬 `rp`媛 ?놁쓣 ??
- `rp`留??덇퀬 `rq`媛 ?놁쓣 ??
- 吏?먰븯吏 ?딅뒗 ??낆씠 ?ㅼ뼱?붿쓣 ??
- 以묐났 `opcode`媛 ?덉쓣 ??
- 以묐났 `content` ?대쫫???덉쓣 ??
- YAML ?뚯떛???ㅽ뙣????

## 沅뚯옣 ?묒뾽 ?먮쫫
1. `Packet/**/*.yaml` ?섏젙
2. `Generate-Packets.cmd` ?ㅽ뻾
3. `Generated/Cpp/Packets/**`, `Generated/CSharp/Packets/**` 蹂寃??뺤씤
4. C++ ?붾（??鍮뚮뱶

## 李멸퀬 臾몄꽌
- [Content Header And Packet CodeGen Plan](D:\Project\ServerPortfolio\RefactoringServer\docs\plans\006_packet-schema-tooling\001_content-header-and-packet-codegen.md)
- [Packet Container Support Policy](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\networklib\packet-container-support-policy.md)
