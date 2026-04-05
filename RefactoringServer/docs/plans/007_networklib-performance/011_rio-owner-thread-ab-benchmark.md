# RIO Owner-Thread A/B Benchmark Plan

## 1. 목적
- `RIO` baseline과 `owner-thread send` 구조를 같은 조건에서 비교한다.
- `RIO`에서 send ownership을 owner worker로 모으는 것이 실제 이득인지 수치로 판단한다.
- 이후 `RIO` 기본 send 정책을 결정한다.

## 2. 비교 대상
이 비교는 별도 backend를 추가하지 않고 `Backend: Rio` 내부 정책만 바꿔서 수행한다.

설정:
- `Backend: Rio`
- `RioSendDispatchMode: Direct | OwnerThread`

의미:
- `Direct`
  - 호출한 스레드에서 바로 `RIOSend()`
- `OwnerThread`
  - `Send()`는 owner worker queue에 enqueue
  - 실제 `RIOSend()`는 owner worker가 수행

## 3. 측정 항목
### 3-1. 처리량
- `overall avg recvTPS`
- `overall avg sendTPS`
- `overall avg recvBps`
- `overall avg sendBps`
- `responses total`
  - 보조 지표

### 3-2. 지연
- `echo-response overall avg / max`
- `room-change-list overall avg / max`
- `room-change overall avg / max`

### 3-3. 자원
- `avg cpuPercent`

## 4. interval=0 재실험 기준
- `interval=0` 비교는 네트워크 send path 차이를 보기 위한 고압 조건으로 다룬다.
- 이 경우 `room-change=90%`는 `RoomFull`과 retry 소모가 너무 커져서 네트워크 비교 노이즈가 된다.
- 따라서 `interval=0` 재실험은 기본적으로 `room-change=10%`를 권장한다.
- 비교 스크립트도 `IntervalMs=0`이면 `RoomChangeProbabilityPercent` 기본값을 `10`으로 사용한다.

## 5. 10분 파일럿 결과
조건:
- `250 sessions`
- `holdSeconds=600`
- `room-count=80`
- `room-capacity=4`
- `room-change=90%`
- recv 관련 timeout `15000ms`

결과:

| Mode | responses total | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: |
| Rio Direct | 619436 | 3.584 ms | 8.107 ms | 9.390 ms |
| Rio OwnerThread | 664640 | 34.702 ms | 41.093 ms | 69.929 ms |
| Iocp | 669984 | 4.228 ms | 9.256 ms | 12.053 ms |

해석:
- `OwnerThread`는 파일럿에서 처리량은 나쁘지 않았지만 RTT가 크게 악화됐다.
- 이 결과만으로 기본 정책을 바꾸긴 어렵고, 더 긴 본실험이 필요했다.

## 6. 2시간 본실험 결과
조건:
- 순서: `RioDirect -> RioOwnerThread -> Iocp`
- `250 sessions`
- `holdSeconds=7200`
- `room-count=80`
- `room-capacity=4`
- `room-change=90%`
- recv 관련 timeout `15000ms`

결과:
- 요약 CSV: [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\dispatch_ab_2h_20260405_035034_2h\summary.csv)

| Mode | responses total | avg recvTPS | avg sendTPS | avg recvBps | avg sendBps | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 1768780 | 714.084 | 714.084 | 10199.501 | 456290.961 | 3.766% | 1.578 ms | 1.446 ms | 4.647 ms |
| Rio OwnerThread | 1767533 | 711.470 | 711.370 | 10168.318 | 454156.445 | 3.729% | 1.470 ms | 1.459 ms | 4.778 ms |
| Iocp | 1767073 | 710.884 | 710.884 | 10164.527 | 453679.689 | 3.700% | 1.464 ms | 1.418 ms | 4.651 ms |

해석:
- `avg TPS/Bps` 기준으로는 `Rio Direct`가 가장 좋았다.
- `OwnerThread`는 10분 파일럿에서 보였던 처리량 우세가 2시간 평균에선 유지되지 않았다.
- RTT는 세 모드가 전반적으로 비슷했고, `OwnerThread`가 기본값으로 넘어갈 만큼 뚜렷한 이점은 보이지 않았다.

## 7. 추가 1시간 4모드 비교
목적:
- `RIO는 SO_SNDBUF=0을 기본`
- `IOCP는 SO_SNDBUF=0 vs -1`
비교를 추가로 확인한다.

비교 대상:
1. `RioDirect` (`SO_SNDBUF=0`)
2. `RioOwnerThread` (`SO_SNDBUF=0`)
3. `IocpSendBuf0`
4. `IocpSendBufDefault` (`-1`)

결과:
- 요약 CSV: [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_1h_20260405_141708_4mode_1h\summary.csv)

| Mode | responses total | avg sendTPS | avg sendBps | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 884249 | 710.855 | 453625.912 | 3.832% | 1.291 ms | 1.504 ms | 4.398 ms |
| Rio OwnerThread | 885203 | 715.116 | 457502.033 | 3.912% | 1.195 ms | 1.519 ms | 4.363 ms |
| IocpSendBuf0 | 884536 | 712.994 | 455413.146 | 3.873% | 1.228 ms | 1.634 ms | 4.941 ms |
| IocpSendBufDefault | 885876 | 713.353 | 455549.521 | 3.904% | 1.116 ms | 1.444 ms | 4.488 ms |

해석:
- 이번 1시간 조건에선 `Rio OwnerThread`가 근소하게 높았지만 차이는 매우 작다.
- `IOCP`에서는 `SO_SNDBUF=-1`이 `0`보다 조금 더 좋게 나왔다.
- 즉 `IOCP`에서 `SO_SNDBUF=0`을 기본값으로 강제할 근거는 약하다.
- `RIO`는 현재 코드 기준 기본 `0`으로 둬도 무난하지만, 이 역시 send path 본체보다 영향은 작다.

## 8. 추가 1시간 4모드 비교 (`interval=0`, `room-change=10%`)
목적:
- 같은 머신에서 서버/클라를 함께 둔 고압 조건에서 네 모드의 상대 순위를 다시 본다.
- `room-change=90%`로 생기던 room-flow 노이즈를 줄이기 위해 `room-change=10%`를 사용한다.

비교 대상:
1. `RioDirect` (`SO_SNDBUF=0`)
2. `RioOwnerThread` (`SO_SNDBUF=0`)
3. `IocpSendBuf0`
4. `IocpSendBufDefault` (`-1`)

조건:
- `250 sessions`
- `holdSeconds=3600`
- `interval=0`
- `room-count=80`
- `room-capacity=4`
- `room-change=10%`
- recv 관련 timeout `15000ms`

결과:
- 실행 루트: [sndbuf_ab_1h_20260405_195951_interval0_room10_4mode_1h](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_1h_20260405_195951_interval0_room10_4mode_1h)

| Mode | responses total | avg sendTPS | avg sendBps | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 20772406 | 7134.390 | 1348782.591 | 12.068% | 1.133 ms | 1.884 ms | 3.584 ms |
| Rio OwnerThread | 19731937 | 6783.761 | 1283450.672 | 11.616% | 1.541 ms | 2.490 ms | 4.261 ms |
| IocpSendBuf0 | 20398384 | 6995.189 | 1317210.658 | 12.106% | 0.927 ms | 1.673 ms | 3.187 ms |
| IocpSendBufDefault | 20591438 | 7085.586 | 1346344.409 | 12.243% | 1.089 ms | 1.826 ms | 4.033 ms |

해석:
- 고압 조건에선 `Rio Direct`가 처리량 기준 1위였다.
- `IocpSendBufDefault`가 그 뒤를 이었고, `IOCP` 내부에선 여전히 `-1`이 `0`보다 throughput이 더 좋았다.
- `IocpSendBuf0`는 RTT는 가장 좋았지만 throughput 손해가 있었다.
- `Rio OwnerThread`는 이번 조건에선 처리량과 RTT 모두 가장 불리했다.

주의:
- 이번 수치는 서버와 클라이언트를 같은 머신에서 동시에 돌린 결과다.
- 따라서 클라이언트가 차지한 CPU 때문에 서버 단독 성능 ceiling이 눌렸을 가능성이 있다.
- 다만 네 모드를 같은 환경에서 상대 비교한 baseline으로는 의미가 있다.

## 9. 현재 결론
- `RIO` 기본 send 정책은 여전히 `Direct`가 더 적절하다.
- `OwnerThread`는 후속 최적화 실험 경로로 유지한다.
- `IOCP`는 `SO_SNDBUF=0`보다 기본값 `-1`이 더 낫거나 최소한 손해가 없다.
- `RIO`는 현재 구현에서 `SO_SNDBUF=0`을 기본으로 두되, 성능 차이의 본질은 send ownership, queue 구조, registered buffer 관리 쪽이라고 본다.
- `interval=0` 단기 스트레스 비교는 `room-change=10%`를 기본으로 삼아 room-flow 노이즈를 줄인다.
- 현재 고압 baseline 순위는 `Rio Direct > IocpSendBufDefault > IocpSendBuf0 > RioOwnerThread`로 본다.
