# EchoServer WindowsServer 2H 4Mode Review

## 1. 목적
- `Windows Server` 4코어 환경에서 수행한 `EchoServer` 2시간 장기 벤치를 정리한다.
- 비교 대상은 `IOCP default`, `IOCP send buffer 0`, `RIO Direct`, `RIO OwnerThread` 4개 모드다.
- 이번 문서의 목적은 `작은 echo payload` 기준에서 각 백엔드의 장기 처리량, 평균 RTT, CPU 효율, 안정성을 비교하는 것이다.

## 2. 테스트 조건
- 결과 루트:
  - [sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt)
- 실행 순서:
  - [sequence.log](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\sequence.log)
- 최종 요약:
  - [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\summary.csv)
- 공통 클라이언트 설정 예시:
  - [EchoClient.IocpSendBufDefault.yaml](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\iocp_sndbuf_default\EchoClient.IocpSendBufDefault.yaml)
- 서버 설정 예시:
  - [EchoServer.IocpSendBufDefault.yaml](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\iocp_sndbuf_default\EchoServer.IocpSendBufDefault.yaml)
  - [EchoServer.RioOwnerThread.yaml](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\rio_owner\EchoServer.RioOwnerThread.yaml)

공통 조건:
- `SessionCount = 250`
- `HoldSeconds = 7200`
- `IntervalMs = 0`
- `PayloadSize = 16`
- `WorkerThreadCount = 4`
- `MaxSessionCount = 512`
- `RoomCount = 80`
- `RoomCapacity = 4`
- `ResponseThreadCount = 1`
- `ResponsesPerThread = 1`

모드별 차이:
- `IocpSendBufDefault`
  - `Backend = Iocp`
  - `SocketSendBufferBytes = -1`
- `IocpSendBuf0`
  - `Backend = Iocp`
  - `SocketSendBufferBytes = 0`
- `RioDirect`
  - `Backend = Rio`
  - `RioSendDispatchMode = Direct`
  - `SocketSendBufferBytes = 0`
- `RioOwnerThread`
  - `Backend = Rio`
  - `RioSendDispatchMode = OwnerThread`
  - `SocketSendBufferBytes = 0`

## 3. 결과 요약
| Mode | ResponsesTotal | AvgRecvTPS | AvgCpuPercent | EchoAvgMs | EchoMaxMs |
| --- | ---: | ---: | ---: | ---: | ---: |
| `RioOwnerThread` | `664,999,733` | `106,417.10` | `42.45%` | `2.675` | `340.707` |
| `IocpSendBufDefault` | `366,659,124` | `52,863.30` | `55.06%` | `4.872` | `118.497` |
| `RioDirect` | `364,318,989` | `51,247.68` | `54.57%` | `4.905` | `270.922` |
| `IocpSendBuf0` | `302,036,099` | `43,302.77` | `59.61%` | `5.920` | `118.760` |

추가 효율 지표:

| Mode | TPS / CPU% |
| --- | ---: |
| `RioOwnerThread` | `2507.07` |
| `IocpSendBufDefault` | `960.05` |
| `RioDirect` | `939.06` |
| `IocpSendBuf0` | `726.46` |

## 4. 해석
### 4-1. 이번 echo 워크로드에서는 `RioOwnerThread`가 가장 강했다
- `IocpSendBufDefault` 대비 `AvgRecvTPS`가 약 `+101.31%` 높았다.
- 같은 기준에서 `AvgCpuPercent`는 약 `-22.91%` 낮았다.
- `EchoAvgMs`도 `4.872ms -> 2.675ms`로 크게 낮았다.
- 장기 효율 지표인 `TPS / CPU%`에서도 `RioOwnerThread`가 가장 높았다.

근거:
- [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\summary.csv)
- [rio_owner\server.log](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\rio_owner\server.log)
- [rio_owner\rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\rio_owner\rtt.csv)

### 4-2. `RioDirect`는 이번 조건에서 `IOCP default`와 거의 동급이었다
- `AvgRecvTPS`는 `IocpSendBufDefault` 대비 약 `-3.06%`였다.
- `AvgCpuPercent`는 약 `-0.89%`로 사실상 비슷했다.
- `EchoAvgMs`는 `4.872ms`와 `4.905ms`로 거의 차이가 없었다.
- 즉 이번 `16B echo / 250 sessions / interval 0 / 2h` 조건에서는 `RioDirect`의 우위가 크지 않았다.

근거:
- [iocp_sndbuf_default\server.log](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\iocp_sndbuf_default\server.log)
- [rio_direct\server.log](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\rio_direct\server.log)
- [iocp_sndbuf_default\rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\iocp_sndbuf_default\rtt.csv)
- [rio_direct\rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\rio_direct\rtt.csv)

### 4-3. `IOCP send buffer 0`는 명확히 불리했다
- `IocpSendBufDefault` 대비 처리량이 약 `-18.09%` 낮았다.
- CPU는 오히려 약 `+8.25%` 높았다.
- 평균 RTT도 `4.872ms -> 5.920ms`로 나빠졌다.
- 이번 테스트 기준으로는 `IOCP`에서 소켓 send buffer를 `0`으로 두는 것은 손해에 가깝다.

근거:
- [iocp_sndbuf_0\server.log](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\iocp_sndbuf_0\server.log)
- [iocp_sndbuf_0\rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\iocp_sndbuf_0\rtt.csv)

### 4-4. `RioOwnerThread`는 평균은 가장 좋지만 tail spike는 가장 컸다
- `EchoAvgMs`는 가장 낮았지만 `EchoMaxMs`는 `340.707ms`로 가장 높았다.
- [rio_owner\rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\rio_owner\rtt.csv) 기준 마지막 분 `minute_avg_ms`는 `0.476ms`까지 내려갔다.
- 반면 `overall_max1_ms`는 `340.707ms`라서, steady-state는 매우 좋지만 드문 long tail이 있었다고 보는 편이 맞다.
- `RioDirect`도 비슷하게 평균은 `4.905ms`지만 최대값은 `270.922ms`로 높았다.
- `IOCP` 두 모드는 평균은 더 높지만 최대값은 `118ms` 수준에서 상대적으로 낮았다.

## 5. 안정성
- 4개 모드 모두 `client.err.log`, `server.err.log`가 `0 byte`였다.
  - [iocp_sndbuf_default\client.err.log](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\iocp_sndbuf_default\client.err.log)
  - [iocp_sndbuf_default\server.err.log](D:\Project\ServerPortfolio\RefactoringServer\WindowsServerTest\sndbuf_ab_7200s_20260407_131530_winserver_4core_2h_rtt\iocp_sndbuf_default\server.err.log)
- 로그에 보이는 대량 `client disconnected`는 각 run 종료 시점 정리 로그로 보이며, 장기 구간 중 비정상 disconnect 증거는 보이지 않았다.
- `RIO` 경로에서도 이번 결과 폴더 기준 `send stall`, `ring full`, `10053`, `10054` 같은 강한 실패 신호는 보이지 않았다.
- `RIO`의 `maxObservedSessionSendRingUsedBytes`도 `1937` 수준이라 이번 `16B echo`에서는 send ring 용량이 병목으로 보이지 않는다.

## 6. 주의
- 4개 모드는 순차 실행이라 완전히 동시에 비교한 결과는 아니다.
- 절대값 비교보다 상대 경향을 보는 용도로 해석하는 편이 안전하다.
- 이번 결과는 `EchoServer`의 작은 payload 왕복 성격이 강해서, `ChattingServer`의 broadcast-heavy 결과와는 분리해서 봐야 한다.

## 7. 결론
- 이번 `WindowsServer 4코어 / EchoServer / 16B / 250 sessions / 2h` 조건에서는 `RioOwnerThread`가 처리량, 평균 RTT, CPU 효율 모두 가장 좋았다.
- `RioDirect`는 이번 조건에선 `IOCP default`와 거의 동급이었다.
- `IOCP send buffer 0`는 4개 중 가장 좋지 않았다.
- 따라서 작은 echo hot path 기준 장기 baseline은 이번 결과를 `RioOwnerThread 우세`로 정리하는 것이 타당하다.

## 8. 다음 액션
- 같은 Windows Server 환경에서 `payload size sweep` 추가
- `session count sweep` 추가
- `RioOwnerThread`의 드문 tail spike 원인 추적
- 같은 조건을 `ChattingServer`에도 맞춰서 `echo vs chatting` 차이 비교
