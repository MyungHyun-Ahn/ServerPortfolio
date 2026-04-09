# ConfigGenerator

媛꾨떒??YAML config schema瑜??쎌뼱??C++ ?ㅼ젙 臾몄꽌/濡쒕뜑 肄붾뱶瑜??앹꽦?섎뒗 ?ㅽ봽?쇱씤 ?꾧뎄??

## ??븷
- `RefactoringServer/ConfigSchema/**/*.schema.yaml` ?ㅽ궎留덈? ?쎈뒗??
- `RefactoringServer/Generated/Cpp/Config/**` ?꾨옒 generated C++ 肄붾뱶瑜?留뚮뱺??
- ?고???YAML怨???묐릺???ㅼ젙 臾몄꽌 援ъ“瑜??섎룞 struct ?묒꽦 ?놁씠 ?좎??쒕떎.

## 湲곕낯 ?ㅽ뻾
?꾨줈?앺듃 猷⑦듃?먯꽌:

```powershell
RefactoringServer\scripts\generate\Generate-Configs.cmd
```

吏곸젒 ?ㅽ뻾:

```powershell
dotnet run --project RefactoringServer\Tools\ConfigGenerator\ConfigGenerator.csproj
```

## ?몄옄
- `--schema-root <path>`
  - 湲곕낯媛? `RefactoringServer/ConfigSchema`
- `--output-root <path>`
  - 湲곕낯媛? `RefactoringServer/Generated/Cpp/Config`
- `--config-root <path>`
  - 湲곕낯媛? `RefactoringServer/Config`

??

```powershell
dotnet run --project RefactoringServer\Tools\ConfigGenerator\ConfigGenerator.csproj -- --schema-root D:\Project\ServerPortfolio\RefactoringServer\ConfigSchema --output-root D:\Project\ServerPortfolio\RefactoringServer\Generated\Cpp\Config --config-root D:\Project\ServerPortfolio\RefactoringServer\Config
```

## ?ㅽ궎留??뺤떇
?뚯씪紐낆뿉??target??異붾줎?쒕떎.

- `ConfigSchema/Server/EchoServer.schema.yaml` -> `EchoServer`
- `ConfigSchema/Client/EchoClient.schema.yaml` -> `EchoClient`

?ㅽ궎留덈뒗 理쒖긽???뱀뀡 留??뺤떇?쇰줈 ?대떎.

```yaml
EchoServer:
  Backend:
    type: enum
    default: Iocp
    values: [Iocp, Rio, BoostAsio]
  Port: { type: uint16, default: 19000 }

Debug:
  Headless: { type: bool, default: false }
  BootstrapTrace: { type: bool, default: false }
```

## 吏???꾨뱶 ?띿꽦
- `type`
- `default`
- `required`
- `description`
- `values`

### `required` ?ъ슜 洹쒖튃
- `required: true`硫?generated loader媛 `ReadRequired*` 寃쎈줈瑜??ъ슜?쒕떎.
- `required + default`??sample YAML??湲곕낯媛믪씠 梨꾩썙吏誘濡?湲곕낯 ?ㅽ뻾???좎??섎㈃???꾩닔 ???뺤콉???쒗쁽?????대떎.
- `required + default ?놁쓬`? ?щ엺??吏곸젒 媛믪쓣 梨꾩썙???섎뒗 ?댁쁺 ?꾩슜 ??ぉ???대떎.

## 吏?????- `bool`
- `int32`, `uint16`, `uint32`, `int64`, `uint64`
- `float`, `double`
- `string`
- `enum`

## ?앹꽦 寃곌낵
- `Generated/Cpp/Config/<Target>/<Target>Config.h`
- `Generated/Cpp/Config/<Target>/<Target>Config.cpp`
- `Config/<relative-directory>/<Target>.yaml`

??
- `Generated/Cpp/Config/EchoServer/EchoServerConfig.h`
- `Generated/Cpp/Config/EchoClient/EchoClientConfig.h`

## ?앹꽦 洹쒖튃
- 猷⑦듃 ?대옒???대쫫? ?먮룞 ?앹꽦?쒕떎.
  - `EchoServer` -> `FEchoServerConfigDocument`
- ?뱀뀡 ?대옒???대쫫???먮룞 ?앹꽦?쒕떎.
  - `EchoServer` -> `SEchoServerConfig`
  - `Debug` -> `SEchoServerDebugConfig`
- enum field??generated enum?쇰줈 ?밴꺽?쒕떎.
- sample YAML? schema 湲곕낯媛믨낵 enum ?덉슜媛?二쇱꽍???④퍡 ?앹꽦?쒕떎.
- `required: true`??field??generated loader?먯꽌 `ReadRequired*` 寃쎈줈瑜??ъ슜?쒕떎.

??

```yaml
EchoServer:
  BindIp: { type: string, default: 127.0.0.1, required: true }
  Port: { type: uint16, default: 19000, required: true }
```

## ?댁쁺 洹쒖튃
1. ?ㅽ궎留덈? ?섏젙?섎㈃ `Generate-Configs.cmd`瑜??ㅼ떆 ?ㅽ뻾?쒕떎.
2. generated C++ 肄붾뱶??而ㅻ컠 ??곸씠??
3. `Tools/ConfigGenerator/bin`, `Tools/ConfigGenerator/obj`??而ㅻ컠 ??곸씠 ?꾨땲??
4. ?쇰컲 C++ ?꾨줈?앺듃 鍮뚮뱶??`ConfigGenerator`瑜??먮룞 ?ㅽ뻾?섏? ?딅뒗??
5. ?꾩슂?섎㈃ [Generate-Codegen.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\generate\Generate-Codegen.cmd)濡?packet/config ?앹꽦湲곕? ??踰덉뿉 ?ㅽ뻾?쒕떎.
