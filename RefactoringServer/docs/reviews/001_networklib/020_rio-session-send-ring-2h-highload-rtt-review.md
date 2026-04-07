# RIO Session Send Ring 2H Highload RTT Review

## 1. 목적
- `session-local send ring`으로 전환한 뒤 `RIO Direct`, `RIO OwnerThread`, `IOCP SendBuf 0`, `IOCP Default` 4개 모드를 같은 조건으로 `2시간` 고부하 검증한 결과를 남긴다.
- 이번 문서의 핵심 질문은 두 가지다.
  - 안정성이 충분한가
  - 기존 `1 packet = 1 send` 구조보다 실제로 좋아졌는가

## 2. 조건
- `250 sessions`
- `holdSeconds=7200`
- `interval=0`
- `room-change=90%`
- `connectsPerSecond=10`
- `Server / Contents / Client worker = 4 / 4 / 4`
- `RTT CSV on`
- `race / sleep injection off`
- `BootstrapTrace=false`
- `LogPackets=false`

실행 결과:
- [RioDirect](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_7200s_20260407_020908_2h_highload_rtt_norace\rio_direct)
- [RioOwnerThread](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_7200s_20260407_041044_2h_highload_rtt_norace_owner\rio_owner)
- [IocpSendBuf0](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_7200s_20260407_061151_2h_highload_rtt_norace_iocp0\iocp_sndbuf_0)
- [IocpSendBufDefault](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_7200s_20260407_081259_2h_highload_rtt_norace_iocpdefault\iocp_sndbuf_default)

## 3. 결과 요약
| Mode | responses total | avg sendTPS | avg CPU | echo avg |
| --- | ---: | ---: | ---: | ---: |
| `Rio Direct` | `121,017,392` | `16,968.333` | `9.347%` | `14.708 ms` |
| `IocpSendBufDefault` | `118,431,548` | `16,603.436` | `10.645%` | `15.030 ms` |
| `IocpSendBuf0` | `117,284,112` | `16,454.106` | `10.752%` | `15.176 ms` |
| `Rio OwnerThread` | `116,984,267` | `16,398.237` | `9.826%` | `15.217 ms` |

bootstrap / steady-stage RTT:

| Mode | login avg | room-list avg | room-enter avg | echo avg |
| --- | ---: | ---: | ---: | ---: |
| `Rio Direct` | `11.022 ms` | `383.505 ms` | `8.246 ms` | `14.708 ms` |
| `Rio OwnerThread` | `11.272 ms` | `417.315 ms` | `8.658 ms` | `15.217 ms` |
| `IocpSendBuf0` | `11.411 ms` | `402.979 ms` | `8.409 ms` | `15.176 ms` |
| `IocpSendBufDefault` | `11.676 ms` | `403.088 ms` | `8.533 ms` | `15.030 ms` |

## 4. 안정성
- 4개 모드 모두 `echo validation succeeded`
- 4개 모드 모두 `client.err.log = 0`
- 4개 모드 모두 `server.err.log = 0`
- RTT CSV 기준 `overall_timeout_count = 0`

즉 이번 `2시간` 고부하 조건에서는 안정성 문제 없이 모두 완주했다.

## 5. 1 Packet = 1 Send 대비 개선 여부
### 5-1. 결론
- **개선됐다.**
- 특히 `Rio Direct`는 이전 `1 packet = 1 send` 계열 비교에선 `IOCP Default`보다 뒤지거나 비슷한 구간이 있었는데, 이번 `session send ring` 전환 후 `2시간` 고부하에서는 4개 모드 중 1위를 기록했다.

### 5-2. 비교 기준
이전 기준 문서:
- [018_networklib-performance-analysis-summary.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\018_networklib-performance-analysis-summary.md)

이전 `1 packet = 1 send` 계열 대표 수치:

1. `2026-04-06` 1시간 4모드 비교
- `IocpSendBufDefault`: `avg sendTPS = 4894.0`
- `IocpSendBuf0`: `avg sendTPS = 4679.6`
- `Rio Direct`: `avg sendTPS = 4681.4`
- `Rio OwnerThread`: `avg sendTPS = 4799.8`

2. `2026-04-07` 10분 RTT 비교
- `Rio Direct`: `avg sendTPS = 3381.424`
- `Rio OwnerThread`: `avg sendTPS = 3374.903`
- `IOCP Default`: `avg sendTPS = 3254.249`

이번 `session send ring` 전환 후 `2026-04-07` 2시간 결과:
- `Rio Direct`: `avg sendTPS = 16968.333`
- `Rio OwnerThread`: `avg sendTPS = 16398.237`
- `IocpSendBufDefault`: `avg sendTPS = 16603.436`
- `IocpSendBuf0`: `avg sendTPS = 16454.106`

### 5-3. 해석
- 절대 배율은 `BootstrapTrace`, `LogPackets`, 런 길이 차이 같은 영향이 섞이므로 조심해서 봐야 한다.
- 그럼에도 불구하고 추세는 분명하다.
  - 이전에는 `RIO`가 `IOCP Default`를 확실히 앞선다고 보기 어려웠다.
  - 지금은 `Rio Direct`가 같은 고부하 비교군에서 가장 높은 처리량과 가장 낮은 CPU를 동시에 기록했다.
- 즉 이번 `session send ring` 전환은 단순 미세 튜닝이 아니라, `RIO send hot path`의 구조적 병목을 실제로 줄인 변경으로 보는 것이 맞다.

## 6. RTT 해석 시 주의점
- 이번 CSV에서는 `room-change-list`, `room-change` stage가 4개 모드 모두 `overall_count = 0`으로 집계됐다.
- 따라서 이번 RTT 비교는 사실상 `login-response`, `room-list`, `room-enter`, `echo-response` 위주로 읽어야 한다.
- 특히 `room-list avg`가 네 모드 모두 `383 ~ 417 ms`로 높게 나온 것은 `RIO`만의 문제라기보다 현재 시나리오/계측 방식의 영향일 가능성이 높다.

## 7. 결론
- `session send ring` 전환 후 `2시간` 고부하 + RTT 수집 검증은 안정성 기준을 통과했다.
- 그리고 이전 `1 packet = 1 send` 구조와 비교하면 성능도 개선됐다.
- 가장 중요한 변화는 `Rio Direct`가 더 이상 추격자가 아니라, 이번 비교에선 **가장 빠른 모드**가 되었다는 점이다.
