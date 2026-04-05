# NetworkLib Performance Analysis Summary

## 1. 목적
- 최근 진행한 `RIO`, `IOCP`, `SO_SNDBUF`, `AcceptEx`, send 경로 최적화 분석 결과를 한 문서에 요약한다.
- 세부 구현 흐름은 개별 리뷰 문서에 두고, 이 문서는 현재 판단과 다음 액션을 빠르게 확인하는 용도로 쓴다.

## 2. 범위
- `RIO` baseline 안정화와 `RIOCreateRequestQueue error=10055` 수정
- `Rio Direct / Rio OwnerThread / Iocp` 비교
- `RIO / IOCP SO_SNDBUF` 비교
- `IOCP AcceptEx` 전환과 재접속 stress 검증
- 기본 `SendPacket` 경로 교체 이후의 현재 판단

관련 문서:
- [013_pure-rio-baseline-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\013_pure-rio-baseline-review.md)
- [015_rio-send-dispatch-mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\015_rio-send-dispatch-mode-review.md)
- [016_iocp-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\016_iocp-echo-server-flow-review.md)
- [017_iocp-acceptex-reconnect-stress-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\017_iocp-acceptex-reconnect-stress-review.md)

## 3. 주요 실험과 결과
### 3-1. RIO baseline 안정화
- 초기 `250세션` 비교 런에서 `RIOCreateRequestQueue failed. error=10055`가 발생했다.
- 원인은 세션 admission 시 예약값이 과했고, 세션 풀 warm-up이 부족했던 점이었다.
- 수정:
  - `FRioSession::EnsurePoolCapacity(maxSessionCount)` 추가
  - `FRioServer`의 `kMaxOutstandingSend`를 `64 -> 8`로 완화
- 수정 후 `Rio Direct / 250세션 / 10분` 런이 정상 성공했다.
- 결과 로그:
  - [client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\client.log)
  - [server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_fix_250x10m_t15_r80\server.log)

### 3-2. 10분 파일럿
조건:
- `250 sessions`
- `holdSeconds=600`
- `room-count=80`
- `room-capacity=4`
- `room-change=90%`
- recv 관련 timeout `15000ms`

| Mode | responses total | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: |
| Rio Direct | 619436 | 3.584 ms | 8.107 ms | 9.390 ms |
| Rio OwnerThread | 664640 | 34.702 ms | 41.093 ms | 69.929 ms |
| Iocp | 669984 | 4.228 ms | 9.256 ms | 12.053 ms |

해석:
- `OwnerThread`는 파일럿에서 RTT 손해가 매우 컸다.
- 이 시점에는 기본 정책을 `Direct`에서 바꿀 근거가 부족했다.

### 3-3. 2시간 본실험
요약:
- `Rio Direct -> Rio OwnerThread -> Iocp`
- `250 sessions`
- `holdSeconds=7200`

결과 CSV:
- [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\dispatch_ab_2h_20260405_035034_2h\summary.csv)

| Mode | responses total | avg sendTPS | avg sendBps | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 1768780 | 714.084 | 456290.961 | 3.766% | 1.578 ms | 1.446 ms | 4.647 ms |
| Rio OwnerThread | 1767533 | 711.370 | 454156.445 | 3.729% | 1.470 ms | 1.459 ms | 4.778 ms |
| Iocp | 1767073 | 710.884 | 453679.689 | 3.700% | 1.464 ms | 1.418 ms | 4.651 ms |

해석:
- `avg TPS/Bps` 기준으론 `Rio Direct`가 가장 좋았다.
- `OwnerThread`는 10분 파일럿에서 보였던 불안정한 모습은 줄었지만, 2시간 평균 기준으로 `Direct`보다 우세하다고 보기 어려웠다.
- RTT는 세 모드가 전반적으로 비슷했고, `Direct`와 `Iocp`가 실용적으로 같은 비교군으로 보였다.

### 3-4. 1시간 4모드 `SO_SNDBUF` 비교
비교 대상:
1. `RioDirect` (`SO_SNDBUF=0`)
2. `RioOwnerThread` (`SO_SNDBUF=0`)
3. `IocpSendBuf0`
4. `IocpSendBufDefault` (`-1`)

주의:
- 이번 비교는 client 기준 `intervalMs=1000`, `reconnectProbabilityPercent=0` 조건이었다.
- 즉 공격적인 `interval=0` 스트레스가 아니라 완만한 steady-state 비교로 해석하는 게 맞다.

결과 CSV:
- [summary.csv](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_1h_20260405_141708_4mode_1h\summary.csv)

| Mode | responses total | avg sendTPS | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 884249 | 710.855 | 1.291 ms | 1.504 ms | 4.398 ms |
| Rio OwnerThread | 885203 | 715.116 | 1.195 ms | 1.519 ms | 4.363 ms |
| IocpSendBuf0 | 884536 | 712.994 | 1.228 ms | 1.634 ms | 4.941 ms |
| IocpSendBufDefault | 885876 | 713.353 | 1.116 ms | 1.444 ms | 4.488 ms |

해석:
- `RIO` 두 모드 차이는 작았다.
- `IOCP`에서는 `SO_SNDBUF=-1`이 `0`보다 throughput과 RTT 모두 조금 더 좋았다.
- 현재 구현 기준으로 `IOCP`에 `SO_SNDBUF=0`을 기본값으로 강제할 근거는 약하다.
- `RIO`는 현재 구현에서 `SO_SNDBUF=0` 기본으로 둬도 무방하지만, 성능 차이의 핵심은 `SO_SNDBUF`보다 send ownership과 queue 구조에 더 가깝다.

### 3-5. interval=0 고압 재실험 기준
- `interval=0`은 send path 차이를 더 강하게 드러내는 조건이라 재실험 가치가 있다.
- 다만 기존 `room-change=90%` 설정은 `RoomFull`, retry exhaustion, room-flow 혼잡을 크게 키워 네트워크 비교 노이즈가 된다.
- 따라서 `interval=0` 단기 비교는 `room-change=10%`를 기본으로 잡는 것이 더 적절하다.
- 비교 스크립트도 `IntervalMs=0`이면 `RoomChangeProbabilityPercent` 기본값을 `10`으로 쓰도록 정리했다.

### 3-6. 1시간 4모드 `interval=0` 비교
비교 대상:
1. `RioDirect`
2. `RioOwnerThread`
3. `IocpSendBuf0`
4. `IocpSendBufDefault`

조건:
- `250 sessions`
- `holdSeconds=3600`
- `interval=0`
- `room-change=10%`
- 서버/클라 같은 머신 동시 실행

결과 루트:
- [sndbuf_ab_1h_20260405_195951_interval0_room10_4mode_1h](D:\Project\ServerPortfolio\RefactoringServer\Out\sndbuf_ab_1h_20260405_195951_interval0_room10_4mode_1h)

| Mode | responses total | avg sendTPS | avg CPU | echo avg | room-change-list avg | room-change avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Rio Direct | 20772406 | 7134.390 | 12.068% | 1.133 ms | 1.884 ms | 3.584 ms |
| Rio OwnerThread | 19731937 | 6783.761 | 11.616% | 1.541 ms | 2.490 ms | 4.261 ms |
| IocpSendBuf0 | 20398384 | 6995.189 | 12.106% | 0.927 ms | 1.673 ms | 3.187 ms |
| IocpSendBufDefault | 20591438 | 7085.586 | 12.243% | 1.089 ms | 1.826 ms | 4.033 ms |

해석:
- 고압 조건 처리량 기준으로는 `Rio Direct`가 가장 좋았다.
- `IOCP` 내부에선 `SO_SNDBUF=-1`이 여전히 `0`보다 throughput이 좋았다.
- `IocpSendBuf0`는 저지연 경향은 있지만 throughput 손해가 있었다.
- `Rio OwnerThread`는 현재 구현 상태에선 고압 조건 기본값 후보로 보기 어렵다.

주의:
- 이번 결과는 서버와 클라이언트를 같은 머신에서 함께 실행한 결과다.
- 따라서 서버 단독 성능 ceiling보다는 상대 순위를 보는 baseline으로 해석하는 것이 맞다.

### 3-7. IOCP AcceptEx 전환과 재접속 stress
결과 문서:
- [017_iocp-acceptex-reconnect-stress-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\017_iocp-acceptex-reconnect-stress-review.md)

핵심 결과:
- `10세션 / 3분 / reconnect 100% / reconnectDelay 250ms` 조건에서 정상 성공
- `AcceptEx completion failed`, `AcceptEx repost failed`, `SO_UPDATE_ACCEPT_CONTEXT failed`, `slot 부족` 모두 `0건`
- 현재 `IOCP`는 `AcceptEx + accept context slot pool` 기준으로 안정화되었다.

추가 판단:
- `accepted socket reuse`는 별도 옵션으로 실험할 수는 있지만, 현재 우선순위는 높지 않다.
- 직접 측정에서도 이득이 크지 않았고, 복잡도와 버그 위험이 더 크다고 본다.

## 4. 현재 결론
- `RIO` 기본 send 정책은 `Direct` 유지가 적절하다.
- `RIO OwnerThread`는 후속 최적화 실험 경로로 유지한다.
- `RIO`는 현재 구현에서 `SO_SNDBUF=0`을 기본으로 둬도 무방하다.
- `IOCP`는 현재 구현 기준 `SO_SNDBUF=0`보다 기본값 `-1`이 더 낫거나 최소한 손해가 없다.
- `IOCP`는 `AcceptEx + accept context slot pool` 기준으로 충분히 안정화되었다.
- 기본 `SendPacket` 경로 교체 후 다음 우선순위는 `broadcast fan-out`과 send copy 감소 구조의 본격 적용이다.
- `interval=0` 비교는 앞으로 `room-change=10%`를 기본으로 해 room-flow 노이즈를 줄인다.
- 현재 같은 머신 고압 baseline 순위는 `Rio Direct > IocpSendBufDefault > IocpSendBuf0 > RioOwnerThread`로 정리한다.

## 5. 다음 작업
- `broadcast packet fan-out` 경로 추가
- `IOCP` send path copy 감소 후 `SO_SNDBUF=0` 재평가
- 가능하면 서버/클라 분리 머신 또는 서버 우선순위 상향 조건으로 `interval=0` 고압 조건 재비교
- `RIO OwnerThread` inbox / drain 정책 최적화 여부 판단
