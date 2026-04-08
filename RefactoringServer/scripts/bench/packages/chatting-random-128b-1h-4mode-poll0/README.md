# Chatting Random 128B 1H 4Mode Poll0

`ChattingServer`와 `ChattingDummyClient`를 대상으로 `1시간 x 4모드` benchmark를 바로 실행하기 위한 패키지다.
배포용 압축 파일에는 실행 파일과 기본 config도 함께 포함해서, 새 환경에 압축을 풀기만 해도 실행할 수 있게 사용한다.

포함된 4개 모드:
- `IOCP Default`
- `IOCP SendBuf 0`
- `RIO Direct`
- `RIO OwnerThread`

공통 설정:
- `PayloadSizeBytes = 128`
- `SessionCount = 250`
- `ConnectsPerSecond = 20`
- `SendIntervalMs = 0`
- `RoomSelectionMode = Random`
- `RoomCount = 20`
- `EventPollMaxCount = 0`
- `MeasureSeconds = 3600`

실행 방법:
- `RefactoringServer` 루트에서:
  - `scripts\bench\packages\chatting-random-128b-1h-4mode-poll0\Run.cmd`
- 또는 패키지 폴더 안에서:
  - `.\Run.cmd`
- 배포용 self-contained 압축을 풀었다면:
  - 압축 루트의 `Run-ChattingRandom128B1H4Mode.cmd`

직접 runner를 실행하려면:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\bench\Run-Benchmark.ps1 `
  -Manifest scripts\bench\packages\chatting-random-128b-1h-4mode-poll0\chatting-random-128b-1h-4mode-poll0.yaml `
  -OutputLabel chatting_random_128b_250_1h_4mode_poll0
```

결과 위치:
- `Out\bench\<timestamp>_chatting_random_128b_250_1h_4mode_poll0\`

필수 조건:
- `Out\ChattingServer.exe`
- `Out\ChattingDummyClient.exe`

참고:
- `EventPollMaxCount = 0`은 더미 클라이언트가 `PollEvents`를 사실상 무제한으로 drain하도록 만든 설정이다.
- `Random` 분포라서 특정 hotspot 방으로 몰리지 않는다.
- 성공 run에서는 benchmark runner가 `client.stdout.log`, `client.stderr.log`를 자동 삭제한다.
