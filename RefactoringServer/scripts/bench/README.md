# Benchmark Scripts

## 1. 紐⑹쟻
- `scripts/bench`??`RefactoringServer`??踰ㅼ튂留덊겕瑜?`PowerShell`濡?諛섎났 ?ㅽ뻾?섍린 ?꾪븳 ?ㅽ겕由쏀듃 ?붾젆?곕━??
- ?꾩옱 1李?吏???쒕굹由ъ삤??`ChattingServer + ChattingDummyClient`?대떎.
- ?ㅽ뻾 ??? 湲곕낯 ?ㅼ젙, run case 議고빀? `YAML manifest`濡??쒖뼱?쒕떎.

## 2. ?꾩옱 援ъ꽦
- [Run-Benchmark.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\Run-Benchmark.ps1)
  - 硫붿씤 ?ㅽ뻾湲?- [Run-Benchmark.cmd](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\Run-Benchmark.cmd)
  - `PowerShell` ?섑띁
- [Benchmark.Common.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\Benchmark.Common.ps1)
  - 怨듯넻 helper
  - ?쒗븳??YAML ?뚯꽌
  - config patch
  - process ?ㅽ뻾/?뺣━
- [Invoke-ChattingScenario.ps1](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\scenarios\Invoke-ChattingScenario.ps1)
  - `Chatting` ?쒕굹由ъ삤 ?대뙌??- [chatting-smoke.yaml](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\manifests\chatting-smoke.yaml)
  - 理쒖냼 smoke ?덉떆 manifest

## 3. 鍮좊Ⅸ ?쒖옉
### PowerShell
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bench\Run-Benchmark.ps1 `
  -Manifest scripts\bench\manifests\chatting-smoke.yaml `
  -OutputLabel smoke
```

### CMD
```bat
scripts\bench\Run-Benchmark.cmd -Manifest scripts\bench\manifests\chatting-smoke.yaml -OutputLabel smoke
```

沅뚯옣 ?ㅽ뻾 ?꾩튂:
- `RefactoringServer` 猷⑦듃?먯꽌 ?ㅽ뻾

?꾩닔 ?좏뻾 議곌굔:
- [ChattingServer.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\ChattingServer.exe)媛 鍮뚮뱶?섏뼱 ?덉뼱???쒕떎.
- [ChattingDummyClient.exe](D:\Project\ServerPortfolio\RefactoringServer\Out\ChattingDummyClient.exe)媛 鍮뚮뱶?섏뼱 ?덉뼱???쒕떎.
- 湲곕낯 template config媛 議댁옱?댁빞 ?쒕떎.
  - [ChattingServer.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Server\ChattingServer.yaml)
  - [ChattingDummy.yaml](D:\Project\ServerPortfolio\RefactoringServer\Config\Client\ChattingDummy.yaml)

## 4. ?ㅽ뻾 諛⑹떇
1. manifest瑜??쎈뒗??
2. `Defaults`? 媛?`Runs[]` ??ぉ??蹂묓빀?쒕떎.
3. run留덈떎 ?꾩떆 `effective.server.yaml`, `effective.client.yaml`??留뚮뱺??
4. ?쒕쾭瑜?headless 紐⑤뱶濡??ㅽ뻾?쒕떎.
5. ?붾? ?대씪?댁뼵?몃? ?ㅽ뻾?쒕떎.
6. 醫낅즺 ??stdout/stderr, ?붿빟 JSON, sequence CSV瑜???ν븳??

## 5. Manifest 援ъ“
### 湲곕낯 ?덉떆
```yaml
Scenario: Chatting
OutputRoot: Out/bench
ContinueOnError: false

Defaults:
  Timing:
    WarmupSeconds: 0
    MeasureSeconds: 5
    CooldownSeconds: 0
    StartupTimeoutSeconds: 10
    RunTimeoutSeconds: 20
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

Runs:
  - Name: iocp_smoke
    Server:
      Overrides:
        ChattingServer.Backend: Iocp
    Client:
      Overrides:
        ChattingDummy.SessionCount: 8
        ChattingDummy.PayloadSizeBytes: 256
```

### 二쇱슂 ?꾨뱶
- `Scenario`
  - ?꾩옱??`Chatting`留?吏??- `OutputRoot`
  - 寃곌낵 ?붾젆?곕━ 猷⑦듃
- `ContinueOnError`
  - `true`硫???run ?ㅽ뙣 ???ㅼ쓬 run 怨꾩냽 吏꾪뻾
- `Defaults`
  - 紐⑤뱺 run??怨듯넻 ?곸슜??湲곕낯媛?- `Runs`
  - ?ㅼ젣 ?쒖감 ?ㅽ뻾??run 紐⑸줉
- `RepeatCount`
  - ?숈씪 run???щ윭 踰?諛섎났?섍퀬 ?띠쓣 ???ъ슜
- `Enabled`
  - ?꾩떆 鍮꾪솢?깊솕??run???ъ슜

## 6. Override 洹쒖튃
- override key??`<Section>.<Key>` ?뺤떇留?吏?먰븳??
- ??
  - `ChattingServer.Backend`
  - `ChattingServer.MaxChatPayloadBytes`
  - `ChattingDummy.SessionCount`
  - `ChattingDummy.RoomSelectionMode`

?꾩옱??template YAML ?덉뿉 ?대? 議댁옱?섎뒗 key留???뼱?????덈떎.
- ?녿뒗 section/key瑜??덈줈 異붽??섎뒗 湲곕뒫? ?꾩쭅 ?녿떎.

## 7. 寃곌낵臾??꾩튂
?덉떆:
- `Out/bench/20260407_164920_smoke_test3/`

sequence 猷⑦듃?먮뒗 ?꾨옒 ?뚯씪???앷릿??
- `manifest.snapshot.yaml`
- `sequence-summary.csv`
- `sequence-summary.json`
- `failed-runs.txt`

媛?run ?붾젆?곕━?먮뒗 ?꾨옒 ?뚯씪???앷릿??
- `effective.server.yaml`
- `effective.client.yaml`
- `server.stdout.log`
- `server.stderr.log`
- `client.stdout.log`
- `client.stderr.log`
- `rtt.csv`
- `run-summary.json`

## 8. ?꾩옱 吏??踰붿쐞
- `Scenario: Chatting`
- `Runs[]` 湲곕컲 ?쒖감 ?ㅽ뻾
- `RepeatCount`
- `ContinueOnError`
- `ChattingDummyClient` 理쒖쥌 summary ?뚯떛
- `ChattingServer`??`ChattingStats`, `ContentStats` 留덉?留?以??뚯떛

## 9. ?꾩옱 ?쒗븳 ?ы빆
- YAML? ?쒖젣?쒗삎 ?뚯꽌?앸떎.
- ?꾩옱 ???섎뒗 踰붿쐞:
  - scalar
  - 以묒꺽 map
  - flat list
  - `Runs:` ?꾨옒 `- Name: ...` 援ъ“
- ?꾩쭅 誘몄????먮뒗 鍮꾧텒??
  - anchor / alias
  - merge key
  - 蹂듭옟??multiline block scalar
  - template???녿뒗 key ?숈쟻 異붽?
- ?꾩옱 吏???쒕굹由ъ삤??`Chatting` ?섎굹肉먯씠??

## 10. ?먯＜ ?곕뒗 ?ㅽ뻾 ??### Smoke
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bench\Run-Benchmark.ps1 `
  -Manifest scripts\bench\manifests\chatting-smoke.yaml `
  -OutputLabel smoke
```

### ?ㅻⅨ 寃곌낵 ?쇰꺼濡????```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bench\Run-Benchmark.ps1 `
  -Manifest scripts\bench\manifests\chatting-smoke.yaml `
  -OutputLabel iocp_baseline
```

## 11. ?ㅼ쓬 ?뺤옣 ?ъ씤??- `Chatting` ?ㅼ젣 鍮꾧탳 manifest 異붽?
  - `Iocp`
  - `Rio Direct`
  - `Rio OwnerThread`
- `RepeatCount=3` ?댁긽 ?ㅽ뿕 preset 異붽?
- `Echo` ?쒕굹由ъ삤 ?대뙌??異붽?
- manifest `Matrix` ?먮룞 ?뺤옣 吏??
