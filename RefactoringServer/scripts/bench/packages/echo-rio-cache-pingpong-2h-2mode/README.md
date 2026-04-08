# Echo RIO Cache Ping-Pong 2H 2Mode

`Windows Server`에서 바로 풀어서 실행할 수 있게 만든 `EchoServer` 전용 비교 패키지다.
목적은 `RIO Direct`와 `RIO OwnerThread` 두 모드만 `2시간`씩 돌리면서,
`cache ping-pong` 관련 계측치가 실제로 어떻게 달라지는지 확인하는 것이다.

포함 모드:
- `RIO Direct`
- `RIO OwnerThread`

기본 조건:
- `HoldSeconds = 7200`
- `SessionCount = 250`
- `PayloadSize = 16`
- `ConnectsPerSecond = 10`
- `WorkerThreadCount = 4`
- `RoomCount = 80`
- `RoomCapacity = 4`
- `SocketSendBufferBytes = 0`

패키지 구성:
- `Run-EchoRioCachePingPong2H.cmd`
- `Run.ps1`
- `Config/Server/EchoServer.template.yaml`
- `Config/Client/EchoClient.template.yaml`
- `Out/EchoServer/EchoServer.exe`
- `Out/EchoClient/EchoClient.exe`

실행 방법:
1. 압축을 원하는 위치에 푼다.
2. 압축을 푼 루트에서 `Run-EchoRioCachePingPong2H.cmd`를 실행한다.

PowerShell로 직접 실행하려면:

```powershell
powershell -ExecutionPolicy Bypass -File .\Run.ps1
```

짧은 스모크 테스트가 필요하면 예를 들어:

```powershell
powershell -ExecutionPolicy Bypass -File .\Run.ps1 `
  -HoldSeconds 10 `
  -SessionCount 8 `
  -ConnectsPerSecond 20 `
  -OutputLabel echo_rio_cache_pingpong_smoke
```

결과 위치:
- `Out/bench/<timestamp>_echo_rio_cache_pingpong_2h_2mode/`

남는 파일:
- `summary.csv`
- `summary.json`
- `<run>/run-summary.json`
- `<run>/server.stdout.log`
- `<run>/server.stderr.log`
- `<run>/client.stdout.log`
- `<run>/client.stderr.log`
- `<run>/rtt.csv`
- `<run>/server_logs/`

`summary.csv`에 포함되는 핵심 항목:
- `AvgRecvTPS`, `AvgSendTPS`, `AvgCpuPercent`
- `EchoAvgMs`, `EchoMaxMs`
- `FinalRioSendPrepareAvgNs`
- `FinalRioSendRingCrossThreadRatePercent`
- `FinalRioDirectSendRingLockWaitAvgNs`
- `FinalRioDirectSendRingLockHoldAvgNs`

해석 포인트:
- `FinalRioSendRingCrossThreadRatePercent`가 높을수록 같은 session send ring을 서로 다른 스레드가 번갈아 만지는 비율이 높다는 뜻이다.
- `FinalRioDirectSendRingLockWaitAvgNs`, `FinalRioDirectSendRingLockHoldAvgNs`는 `Direct` 경로에서 lock 자체 비용이 어느 정도인지 보는 데 도움 된다.
- `EchoAvgMs`와 `AvgRecvTPS`를 같이 보면 단순 RTT뿐 아니라 전체 echo hot path 처리 효율을 같이 볼 수 있다.
