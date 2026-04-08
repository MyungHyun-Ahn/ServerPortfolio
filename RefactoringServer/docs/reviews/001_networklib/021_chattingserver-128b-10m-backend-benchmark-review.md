# ChattingServer 128B 10M Backend Benchmark Review

## 1. 목적
- `ChattingServer`에서 일반적인 채팅 메시지 크기에 가까운 `128B payload` 기준으로 `IOCP`, `RIO Direct`, `RIO OwnerThread`를 `10분` 동안 비교한 결과를 정리한다.
- 이번 문서는 `8KiB` 큰 패킷 stress test와 분리해서, 실제 채팅에 가까운 조건에서 어떤 백엔드가 더 유리한지 확인하는 데 목적이 있다.

## 2. 조건
- manifest:
  - [chatting-rio-vs-iocp-128b-10m.yaml](D:\Project\ServerPortfolio\RefactoringServer\scripts\bench\manifests\chatting-rio-vs-iocp-128b-10m.yaml)
- 결과 루트:
  - [20260407_181046_rio_vs_iocp_128b_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m)
- 공통 설정:
  - `MeasureSeconds = 600`
  - `PayloadSizeBytes = 128`
  - `MaxChatPayloadBytes = 256`
  - `SessionCount = 150`
  - `ConnectsPerSecond = 50`
  - `SendIntervalMs = 0`
  - `RoomSelectionMode = Hotspot`
  - `HotspotRoomIds = 77`
  - `HotspotBiasPercent = 90`
  - `RoomCount = 50`
  - `RoomCapacity = 256`
  - `RioSendRingSizeBytes = 65536`
- 실행 결과:
  - [sequence-summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\sequence-summary.csv)
  - [iocp_default_128b_150_hotspot_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\iocp_default_128b_150_hotspot_10m)
  - [rio_direct_128b_150_hotspot_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_direct_128b_150_hotspot_10m)
  - [rio_owner_128b_150_hotspot_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_owner_128b_150_hotspot_10m)

## 3. 측정 기준
- 주 처리량 지표는 `chattingSuccess / elapsedSeconds`로 계산한다.
  - 이번 더미는 `ChattingRq`를 보낸 뒤 `ChattingRp`를 기다리는 구조라, 총 성공 건수를 전체 시간으로 나눈 값이 가장 비교하기 쉽다.
  - 관련 코드:
    - [Main.cpp#L1135](D:\Project\ServerPortfolio\RefactoringServer\ChattingDummyClient\Main.cpp#L1135)
    - [Main.cpp#L1138](D:\Project\ServerPortfolio\RefactoringServer\ChattingDummyClient\Main.cpp#L1138)
- `broadcast avg/s`는 `broadcastReceive / elapsedSeconds`로 계산한다.
- RTT는 각 `rtt.csv`에서 `stage = chatting-response`의 **마지막 누적 행** 기준 `overall_avg_ms`를 사용한다.
  - [iocp rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\iocp_default_128b_150_hotspot_10m\rtt.csv)
  - [rio_direct rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_direct_128b_150_hotspot_10m\rtt.csv)
  - [rio_owner rtt.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_owner_128b_150_hotspot_10m\rtt.csv)
- `sequence-summary.csv`의 `sendTPS`, `recvTPS`, `cpuPercent`는 종료 시점 snapshot 성격이 있으므로 보조 지표로만 사용한다.

## 4. 결과 요약
| Mode | chattingSuccess | chat avg/s | broadcastReceive | broadcast avg/s | chatting RTT avg | chatting RTT max1 | reconnect | unexpectedDisconnect |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `IOCP` | `91,831` | `153.05` | `10,288,760` | `17,147.36` | `959.506 ms` | `1388.835 ms` | `0` | `0` |
| `RIO Direct` | `99,324` | `165.53` | `11,016,743` | `18,360.63` | `887.294 ms` | `1065.032 ms` | `0` | `0` |
| `RIO OwnerThread` | `88,142` | `146.90` | `9,871,537` | `16,452.01` | `999.514 ms` | `1080.473 ms` | `0` | `0` |

부가 지표:

| Mode | final cpuPercent | final workingSetMB | 관찰된 send ring 사용량 |
| --- | ---: | ---: | --- |
| `IOCP` | `0.38%` | `28.73 MB` | 해당 없음 |
| `RIO Direct` | `3.61%` | `27.81 MB` | 로그 기준 `maxObservedSessionSendRingUsedBytes = 1217` |
| `RIO OwnerThread` | `0.67%` | `37.81 MB` | 로그 기준 `maxObservedSessionSendRingUsedBytes = 2324` |

로그 근거:
- [rio_direct server.stdout.log](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_direct_128b_150_hotspot_10m\server.stdout.log)
- [rio_owner server.stdout.log](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_owner_128b_150_hotspot_10m\server.stdout.log)

## 5. 해석
### 5-1. 이번 10분 조건에서는 `RIO Direct`가 가장 좋다
- `chat avg/s` 기준으로 `RIO Direct`는 `IOCP` 대비 약 `8.15%` 높다.
- `broadcast avg/s`도 `RIO Direct`가 가장 높다.
- RTT도 `RIO Direct`가 가장 낮다.
  - `IOCP` 대비 평균 RTT 약 `7.53%` 개선
  - `RIO OwnerThread` 대비 평균 RTT 약 `11.23%` 개선

### 5-2. `RIO OwnerThread`는 안정적이지만, 이번 조건에서는 handoff 비용이 더 크게 보인다
- `RIO Direct`는 송신 호출 시점에 바로 ring append 후 `PostSend`로 이어진다.
  - [FRioServer.cpp#L234](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L234)
  - [FRioServer.cpp#L239](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L239)
- `RIO OwnerThread`는 먼저 owner queue에 적재한 뒤, owner worker가 나중에 drain 하면서 `AppendPacketToSendRing`과 `PostSend`를 수행한다.
  - [FRioServer.cpp#L234](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L234)
  - [FRioServer.cpp#L803](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L803)
  - [FRioServer.cpp#L815](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Servers\Core\FRioServer.cpp#L815)
- 이번 더미는 세션당 `1 outstanding chat` 구조라서, `ChattingRp`가 늦어지면 다음 `ChattingRq`도 늦어진다.
  - 즉 per-message handoff 비용이 RTT와 처리량에 직접 반영된다.

### 5-3. `128B`에서는 send ring이 병목이 아니었다
- 세 모드 모두 `reconnect = 0`, `unexpectedDisconnect = 0`, `timeout = 0`, `permanentFailure = 0`으로 끝났다.
- `RIO OwnerThread` 로그에도 `RIO send stall detected`가 없었다.
  - [rio_owner server.stdout.log](D:\Project\ServerPortfolio\RefactoringServer\Out\bench\20260407_181046_rio_vs_iocp_128b_10m\rio_owner_128b_150_hotspot_10m\server.stdout.log)
- 관찰된 ring 사용량도 `1217B`, `2324B` 수준이라 `64KiB` ring은 충분했다.
- 따라서 이번 결과 차이는 `send ring 용량 부족`보다 `dispatch path 차이`와 `worker scheduling` 영향으로 보는 편이 맞다.

## 6. 주의
- 이전에 `rtt.csv`의 첫 번째 `chatting-response` 행만 보면 `OwnerThread`가 더 낮아 보일 수 있었는데, 그 값은 초기 1분 누적값일 뿐이다.
- 최종 비교는 반드시 `rtt.csv`의 마지막 `chatting-response` 행을 기준으로 해야 한다.
- 같은 이유로 `sequence-summary.csv`의 마지막 `sendTPS` 하나만 보고 전체 처리량 우열을 판단하면 왜곡될 수 있다.

## 7. 결론
- `128B / 150 sessions / hotspot 90% / 10분` 조건에서는 `RIO Direct`가 가장 좋은 균형을 보였다.
  - 가장 높은 `chat avg/s`
  - 가장 높은 `broadcast avg/s`
  - 가장 낮은 `chatting-response RTT`
- `RIO OwnerThread`는 큰 패킷 stress 상황과 달리 이번 조건에서 안정성 문제는 없었지만, 처리량과 RTT 모두 `RIO Direct`보다 뒤처졌다.
- 따라서 앞으로 `일반 채팅 크기` 기준 비교의 baseline은 이번 `128B 10분` 결과로 두고, `8KiB`는 별도의 stress / send-ring 한계 검증으로 분리해서 보는 것이 적절하다.

## 8. 다음 액션
- `128 / 256 / 512 / 1024B` payload sweep
- `SessionCount` sweep
- `RIO OwnerThread`가 불리해지는 구간에서 owner queue backlog와 worker scheduling 지표 추가
