# PowerShell BenchmarkRunner Plan

## 1. 紐⑹쟻
- `RefactoringServer`??踰ㅼ튂留덊겕 ?ㅽ뻾???섎룞 ?쒖꽌 ?ㅽ뻾?먯꽌 踰쀬뼱??`PowerShell` ?ㅽ겕由쏀듃 湲곕컲 ?먮룞 ?ㅽ뻾?쇰줈 ?뺣━?쒕떎.
- ?ㅽ뻾 ??곸? 1李⑤줈 `ChattingServer + ChattingDummyClient`?댁?留? 援ъ“??`Echo`, ?ν썑 ?ㅻⅨ ?붾? ?대씪?댁뼵?? ?ㅻⅨ ?쒕쾭 ?섑뵆?먮룄 ?ъ궗??媛?ν븳 踰붿슜 ?щ꼫濡??ㅺ퀎?쒕떎.
- ?ㅽ뻾???ㅽ뿕 議고빀? 肄붾뱶 ?섏젙???꾨땲??`YAML manifest` ?뚯씪濡??쒖뼱?쒕떎.

## 2. 諛곌꼍
- ?꾩옱 ?덊룷?먮뒗 [Run-RioDispatchComparisonSequence.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-RioDispatchComparisonSequence.ps1), [Run-SndBufBackendComparisonSequence.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\Run-SndBufBackendComparisonSequence.ps1)泥섎읆 ?뱀젙 ?ㅽ뿕???꾪빐 ?묒꽦???쒖감 ?ㅽ뻾 ?ㅽ겕由쏀듃媛 ?대? 議댁옱?쒕떎.
- ???ㅽ겕由쏀듃?ㅼ? `config template 蹂듭궗`, `YAML scalar patch`, `?꾨줈?몄뒪 ?ㅽ뻾`, `濡쒓렇 ?섏쭛`, `寃곌낵 ?대뜑 ?앹꽦` 媛숈? ?듭떖 ?⑦꽩???대? 媛뽮퀬 ?덈떎.
- ?섏?留??꾩옱 援ъ“???ㅽ뿕 ??곴낵 吏?쒓? ?ㅽ겕由쏀듃???섎뱶肄붾뵫?섏뼱 ?덉뼱, `ChattingServer`泥섎읆 ?덈줈???쒕굹由ъ삤瑜?異붽????뚮쭏??蹂꾨룄 ?ㅽ겕由쏀듃瑜?蹂듭젣?섎뒗 諛⑺뼢?쇰줈 ?먮? 媛?μ꽦???믩떎.
- `ChattingDummyClient`媛 以鍮꾨맂 ?쒖젏遺?곕뒗 ?숈씪???ㅽ뿕??`RIO`, `IOCP`, ??payload, room selection mode 議고빀蹂꾨줈 諛섎났 ?ㅽ뻾?????덈뒗 ?곸쐞 ?ㅽ뻾湲곌? ?꾩슂?섎떎.

## 3. 紐⑺몴
- `PowerShell` 湲곕컲 `BenchmarkRunner`瑜?異붽??쒕떎.
- `YAML manifest` 1媛쒕줈 ?쒕줈 ?ㅻⅨ `N媛?run case`瑜??쒖감 ?ㅽ뻾?????덉뼱???쒕떎.
- 怨듯넻 ?ㅽ뻾湲곗? ?쒕굹由ъ삤蹂??대뙌?곕? 遺꾨━?쒕떎.
- 1李??대뙌?곕뒗 `ChattingScenario`濡?援ы쁽?쒕떎.
- 寃곌낵??`run directory` ?⑥쐞濡??꾩뭅?대툕?섍퀬, 理쒖쥌 吏묎퀎??`csv/json`?쇰줈 ?④릿??

## 4. 鍮꾨ぉ??- 1李⑥뿉 ?ㅼ떆媛?????쒕낫?쒓퉴吏 留뚮뱾吏 ?딅뒗??
- 1李⑥뿉 ?ㅼ쨷 癒몄떊 遺꾩궛 踰ㅼ튂留덊겕瑜?吏?먰븯吏 ?딅뒗??
- 1李⑥뿉 紐⑤뱺 YAML 湲곕뒫(anchor, merge key, multiline block scalar)??吏?먰븯吏 ?딅뒗??
- 1李⑥뿉 踰ㅼ튂留덊겕 ?щ꼫 ?먯껜瑜?蹂꾨룄 C++ 諛붿씠?덈━濡?留뚮뱾吏 ?딅뒗??

## 5. ?듭떖 寃곗젙
- ?ㅽ뻾湲곕뒗 `PowerShell`濡?留뚮뱺??
- ?ㅽ뿕 ?뺤쓽??`YAML manifest`濡??붾떎.
- `BenchmarkRunner Core`???ㅽ뻾 ?쒖꽌? 怨듯넻 ?섏쭛留??대떦?쒕떎.
- ?쒕쾭/?대씪?댁뼵?몃퀎 config patch, readiness ?먮떒, 寃곌낵 ?붿빟 ?뚯떛? `Scenario Adapter`媛 ?대떦?쒕떎.
- 1李⑤뒗 `Runs: []` 諛곗뿴??紐낆떆?곸쑝濡??쒖감 ?ㅽ뻾?쒕떎.
- 議고빀 ?먮룞 ?앹꽦??`Matrix`???꾩냽 ?④퀎濡??붾떎.

## 6. ?꾩껜 援ъ“
### 6.1 ?붾젆?곕━
- `scripts/bench/Run-Benchmark.ps1`
- `scripts/bench/Benchmark.Common.ps1`
- `scripts/bench/scenarios/Invoke-ChattingScenario.ps1`
- `scripts/bench/manifests/*.yaml`
- `Out/bench/<timestamp>/<run-name>/...`

### 6.2 ??븷 遺꾨━
- `Run-Benchmark.ps1`
  - manifest 濡쒕뱶
  - 怨듯넻 defaults 蹂묓빀
  - `Runs` 諛곗뿴 ?쒖감 ?ㅽ뻾
  - ?ㅽ뙣 ?뺤콉 泥섎━
  - ?꾩껜 吏묎퀎 ?뚯씪 湲곕줉
- `Benchmark.Common.ps1`
  - YAML patch helper
  - ?꾩떆 config ?앹꽦
  - ?꾨줈?몄뒪 ?쒖옉/醫낅즺
  - timeout / orphan cleanup
  - stdout/stderr redirect
  - 寃곌낵 ?붾젆?곕━ 援ъ꽦
- `Invoke-ChattingScenario.ps1`
  - `ChattingServer.yaml`, `ChattingDummy.yaml` template 湲곕컲 effective config ?앹꽦
  - ?쒕쾭 readiness ?湲?  - ?붾? ?ㅽ뻾
  - `summary`, `RTT CSV`, `server log` ?뚯떛

## 7. Manifest 紐⑤뜽
### 7.1 1李?援ъ“
- `Scenario`
- `OutputRoot`
- `ContinueOnError`
- `Defaults`
  - ?쒕쾭 怨듯넻 湲곕낯媛?  - ?대씪?댁뼵??怨듯넻 湲곕낯媛?  - ??대컢 怨듯넻 湲곕낯媛?- `Runs`
  - ?대쫫
  - ?쒓렇
  - enabled ?щ?
  - ?쒕쾭 override
  - ?대씪?댁뼵??override
  - 諛섎났 ?잛닔

### 7.2 ?덉떆
```yaml
Scenario: Chatting
OutputRoot: Out/bench
ContinueOnError: true

Defaults:
  Timing:
    WarmupSeconds: 10
    MeasureSeconds: 60
    CooldownSeconds: 5
    StartupTimeoutSeconds: 10
    ShutdownTimeoutSeconds: 10
  Server:
    Executable: Out/ChattingServer/ChattingServer.exe
    ConfigTemplate: Config/Server/ChattingServer.yaml
    Overrides:
      ChattingServer.Port: 19100
      Debug.Headless: true
  Client:
    Executable: Out/ChattingDummyClient/ChattingDummyClient.exe
    ConfigTemplate: Config/Client/ChattingDummy.yaml
    Overrides:
      ChattingDummy.ServerIp: 127.0.0.1
      ChattingDummy.Port: 19100
      ChattingDummy.PacketKey: 55
      ChattingDummy.RunSeconds: 60

Runs:
  - Name: iocp_8k_200_hotspot
    Tags: [iocp, large-packet, hotspot]
    RepeatCount: 3
    Server:
      Overrides:
        ChattingServer.Backend: Iocp
        ChattingServer.MaxChatPayloadBytes: 8000
    Client:
      Overrides:
        ChattingDummy.SessionCount: 200
        ChattingDummy.SendIntervalMs: 0
        ChattingDummy.PayloadSizeBytes: 8000
        ChattingDummy.RoomSelectionMode: Hotspot
        ChattingDummy.HotspotRoomIds: "77"
        ChattingDummy.HotspotBiasPercent: 80

  - Name: rio_direct_8k_200_hotspot
    Tags: [rio, large-packet, hotspot]
    RepeatCount: 3
    Server:
      Overrides:
        ChattingServer.Backend: Rio
        ChattingServer.RioSendDispatchMode: Direct
        ChattingServer.MaxChatPayloadBytes: 8000
    Client:
      Overrides:
        ChattingDummy.SessionCount: 200
        ChattingDummy.SendIntervalMs: 0
        ChattingDummy.PayloadSizeBytes: 8000
        ChattingDummy.RoomSelectionMode: Hotspot
        ChattingDummy.HotspotRoomIds: "77"
        ChattingDummy.HotspotBiasPercent: 80
```

## 8. ?ㅽ뻾 ?섎챸二쇨린
1. manifest瑜??쎄퀬 `Defaults + Run Override`瑜?蹂묓빀?쒕떎.
2. ?꾩껜 ?ㅽ뻾 猷⑦듃 ?붾젆?곕━瑜??앹꽦?쒕떎.
3. 媛?run留덈떎 怨좎쑀 寃곌낵 ?붾젆?곕━瑜?留뚮뱺??
4. server/client effective config ?뚯씪??寃곌낵 ?붾젆?곕━???앹꽦?쒕떎.
5. ?쒕쾭瑜?headless 紐⑤뱶濡??ㅽ뻾?쒕떎.
6. readiness timeout ?숈븞 ?쒕쾭媛 ?뺤긽 湲곕룞?덈뒗吏 ?뺤씤?쒕떎.
7. ?붾? ?대씪?댁뼵?몃? ?ㅽ뻾?쒕떎.
8. run 醫낅즺 ?먮뒗 timeout源뚯? ?湲고븳??
9. stdout/stderr, effective config, RTT CSV, log file, summary file??寃곌낵 ?붾젆?곕━??蹂닿??쒕떎.
10. ?쒕쾭? ?대씪?댁뼵???꾨줈?몄뒪瑜??뺣━?쒕떎.
11. ?ㅼ쓬 run?쇰줈 ?대룞?쒕떎.

## 9. 踰붿슜???ㅺ퀎 ?먯튃
### 9.1 Runner Core??梨꾪똿??紐⑤Ⅸ??- core??`ChattingServer`, `EchoServer`, `WorldServer` 媛숈? ?대쫫???섎뱶肄붾뵫?섏? ?딅뒗??
- core??`ProcessSpec`, `ConfigTemplate`, `Overrides`, `Timing`, `ArtifactPolicy` 媛숈? 怨듯넻 媛쒕뀗留??덈떎.

### 9.2 Scenario Adapter媛 ?꾨찓?몄쓣 ?덈떎
- `ChattingScenario`??`ChattingServer` config key ?대쫫???덈떎.
- `EchoScenario`??`EchoServer` config key ?대쫫???덈떎.
- 寃곌낵 ?뚯떛??scenario蹂꾨줈 遺꾨━?쒕떎.

### 9.3 湲곗〈 ?ㅽ겕由쏀듃 ?ъ궗??- 湲곗〈 `Run-RioDispatchComparisonSequence.ps1`, `Run-SndBufBackendComparisonSequence.ps1`??`Set-YamlScalarValue`? process orchestration ?⑦꽩??怨듯넻 helper濡??뚯뼱?щ┛??
- ???щ꼫媛 ?덉젙?붾릺硫?湲곗〈 ?⑤컻??鍮꾧탳 ?ㅽ겕由쏀듃瑜??먯쭊?곸쑝濡?manifest 湲곕컲 ?몄텧濡??닿??쒕떎.

## 10. Chatting 1李??곸슜 踰붿쐞
### 10.1 ?쒕쾭 痢??쒖뼱 ??ぉ
- `ChattingServer.Backend`
- `ChattingServer.RioSendDispatchMode`
- `ChattingServer.WorkerThreadCount`
- `ChattingServer.MaxSessionCount`
- `ChattingServer.ContentsWorkerThreadCount`
- `ChattingServer.RoomCount`
- `ChattingServer.RoomCapacity`
- `ChattingServer.MaxChatPayloadBytes`
- `ChattingServer.SocketSendBufferBytes`
- `Debug.Headless`
- `ChattingServer.LogOutputDirectory`

### 10.2 ?붾? 痢??쒖뼱 ??ぉ
- `ChattingDummy.SessionCount`
- `ChattingDummy.ConnectsPerSecond`
- `ChattingDummy.RunSeconds`
- `ChattingDummy.SendIntervalMs`
- `ChattingDummy.PayloadSizeBytes`
- `ChattingDummy.RoomSelectionMode`
- `ChattingDummy.HotspotRoomIds`
- `ChattingDummy.HotspotBiasPercent`
- `ChattingDummy.RoomChangeProbabilityPercent`
- `ChattingDummy.ReconnectProbabilityPercent`
- `ChattingDummy.ReconnectDelayMs`
- `ChattingDummy.ResponseTimeoutMs`
- `ChattingDummy.RttCsvPath`

### 10.3 1李??섏쭛 吏??- `connectSuccess`
- `loginSuccess`
- `roomListResponses`
- `roomChangeSuccess`
- `roomChangeFailure`
- `chattingSend`
- `chattingSuccess`
- `chattingReject`
- `broadcastReceive`
- `reconnect`
- `unexpectedDisconnect`
- `timeout`
- `selfBroadcast`
- `invalidRoomBroadcast`
- `payloadValidationFailure`
- `permanentFailure`
- `RTT p50/p95/p99`
- server log 湲곕컲 `sendTPS`, `recvTPS`, `cpuPercent`, `workingSetMB`

## 11. 寃곌낵臾?援ъ“
### 11.1 run ?붾젆?곕━
- `manifest.snapshot.yaml`
- `effective.server.yaml`
- `effective.client.yaml`
- `server.stdout.log`
- `server.stderr.log`
- `client.stdout.log`
- `client.stderr.log`
- `rtt.csv`
- `run-summary.json`

### 11.2 sequence 猷⑦듃
- `sequence-summary.csv`
- `sequence-summary.json`
- `failed-runs.txt`

## 12. ?ㅽ뙣 ?뺤콉
- `ContinueOnError`
  - true硫???run ?ㅽ뙣 ???ㅼ쓬 run 怨꾩냽 吏꾪뻾
  - false硫?利됱떆 以묐떒
- `StartupTimeoutSeconds`
  - ?쒕쾭 湲곕룞 ?ㅽ뙣 ?먮떒 ?쒓컙
- `ShutdownTimeoutSeconds`
  - graceful 醫낅즺 ?湲??쒓컙
- `RunTimeoutSeconds`
  - client hang ?먮뒗 server stall ?鍮?hard timeout
- orphan process媛 ?⑥쑝硫?媛뺤젣 醫낅즺 ???ㅽ뙣 湲곕줉

## 13. Manifest / YAML ?꾨왂
- 1李?manifest???щ엺??吏곸젒 ?쎄퀬 ?몄쭛?섍린 ?ъ슫 `YAML` ?뺤떇???좎??쒕떎.
- 1李?吏??踰붿쐞??scalar, flat list, 2?④퀎 map 以묒떖???쒗븳??援ъ“濡??붾떎.
- 蹂듭옟??YAML 湲곕뒫? 吏??踰붿쐞?먯꽌 ?쒖쇅?쒕떎.
- ?щ꼫 蹂몄껜??`manifest object` 湲곕컲?쇰줈 ?숈옉?섍쾶 留뚮뱾怨? manifest loader 援ы쁽? 援먯껜 媛?ν븯寃??좎??쒕떎.
- 利?`YAML loader` 援ы쁽 諛⑹떇??諛붾뚮뜑?쇰룄 `Run-Benchmark.ps1`???ㅽ뻾 肄붿뼱???좎??섎룄濡??ㅺ퀎?쒕떎.

## 14. 援ы쁽 ?쒖꽌
1. `scripts/bench` ?붾젆?곕━? 怨듯넻 helper 堉덈?瑜?異붽??쒕떎.
2. 湲곗〈 鍮꾧탳 ?ㅽ겕由쏀듃??YAML patch / process control helper瑜?怨듯넻 紐⑤뱢濡?異붿텧?쒕떎.
3. manifest loader瑜?遺숈씤??
4. `Runs[]` ?쒖감 ?ㅽ뻾 猷⑦봽瑜?援ы쁽?쒕떎.
5. `ChattingScenario` adapter瑜?異붽??쒕떎.
6. `run-summary.json`, `sequence-summary.csv`瑜?湲곕줉?쒕떎.
7. `ChattingServer / ChattingDummyClient`?????manifest瑜?異붽??쒕떎.
8. 湲곗〈 `Echo` 鍮꾧탳 ?ㅽ겕由쏀듃瑜???runner 湲곕컲?쇰줈 ?먯쭊 ?닿??쒕떎.

## 15. 湲곕? ?④낵
- `RIO / IOCP` 鍮꾧탳瑜??щ엺 ?먯쑝濡?諛섎났 ?ㅽ뻾?섏? ?딆븘???쒕떎.
- ?ㅽ뿕 耳?댁뒪 異붽?媛 ?ㅽ겕由쏀듃 蹂듭젣媛 ?꾨땲??manifest 異붽?濡?諛붾먮떎.
- `Chatting`肉??꾨땲???ㅻⅨ ?깅뒫 ?ㅽ뿕?먮룄 媛숈? ?ㅽ뻾湲곕? ?ъ궗?⑺븷 ???덈떎.
- 踰ㅼ튂留덊겕 ?ы쁽?깃낵 ?ㅽ뻾 湲곕줉 蹂댁〈 ?섏????щ씪媛꾨떎.

