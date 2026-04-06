# NetworkLib Performance Analysis Summary

## 1. 목적
- 최근 진행한 `RIO`, `IOCP`, `SO_SNDBUF`, `AcceptEx`, send 경로 최적화 결과를 한 문서에 요약한다.
- 개별 리뷰 문서의 핵심 결론과 현재 우선순위를 빠르게 확인하는 용도로 사용한다.

## 2. 범위
- `RIO` baseline 안정화
- `Rio Direct / Rio OwnerThread / Iocp` 비교
- `SO_SNDBUF` 비교
- `IOCP AcceptEx` 안정성 검증
- `RIO Registered Buffer Pool` 전환 이후 최신 결과

관련 문서:
- [013_pure-rio-baseline-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\013_pure-rio-baseline-review.md)
- [015_rio-send-dispatch-mode-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\015_rio-send-dispatch-mode-review.md)
- [016_iocp-echo-server-flow-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\016_iocp-echo-server-flow-review.md)
- [017_iocp-acceptex-reconnect-stress-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\001_networklib\017_iocp-acceptex-reconnect-stress-review.md)

## 3. 주요 결과
### 3-1. RIO baseline 안정화
- 초기 `250세션` 비교 중 `RIOCreateRequestQueue failed. error=10055`가 발생했다.
- 원인은 세션 admission 대비 send outstanding budget이 과하게 컸고, session pool warm-up도 부족했던 쪽으로 정리했다.
- 수정:
  - `FRioSession::EnsurePoolCapacity(maxSessionCount)` 추가
  - `FRioServer`의 `kMaxOutstandingSend`를 `64 -> 8`로 완화
- 수정 후 `Rio Direct / 250세션 / 10분` 런은 정상 성공했다.

### 3-2. 초기 dispatch / backend 비교 요약
- 짧은 10분 스트레스 기준에서는 `OwnerThread`가 RTT 열화가 컸다.
- 장시간 평균에서는 `Rio Direct`, `Rio OwnerThread`, `Iocp`가 서로 근접한 구간도 있었지만, `OwnerThread`를 기본 경로로 올릴 만큼의 이점은 확인하지 못했다.
- 결론은 `RIO` 기본 send 경로는 `Direct`가 더 실용적이라는 쪽이다.

### 3-3. IOCP AcceptEx 전환 결과
- `10세션 / 3분 / reconnect 100% / reconnectDelay 250ms` 조건에서 정상 성공했다.
- `AcceptEx completion failed`, `AcceptEx repost failed`, `SO_UPDATE_ACCEPT_CONTEXT failed`는 재현되지 않았다.
- 현재 `IOCP`는 `AcceptEx + accept context slot pool` 기준으로 충분히 안정화된 상태로 본다.

### 3-4. RIO Registered Buffer Pool 전환
- 기존 `RIO send`는 `per-send register/deregister` 구조였다.
- 현재는 `64 KiB region + bucket 기반 fixed-slot allocator` 위에 `RIO`일 때만 registered metadata를 얹는 공용 send segment pool 구조로 전환했다.
- 구현 후 `IOCP`, `RIO Direct`, `RIO OwnerThread` 짧은 스모크를 모두 통과했다.
- 전환 중 발견된 `registered region BufferId를 completion마다 deregister하던 버그`도 함께 수정했다.

### 3-5. 최신 10분 RIO 고부하 검증 (`2026-04-06`)
조건:
- `Rio Direct`
- `250 sessions`
- `holdSeconds=600`
- `interval=0`
- `room-change=90%`
- `connectsPerSecond=10`
- `Server / Contents / Client worker = 4 / 4 / 4`
- `ContentsRuntime` 인위적 race / sleep 주입 비활성

결과:
- [rio_highload_250x10m_interval0_no_race](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_highload_250x10m_interval0_no_race)
- `echo validation succeeded. sessions=250 responses=734891`
- `client/server err log` 비어 있음
- 종료 시점 `enqueueFailTPS=0`, `roomMaxQueue=20`, `roomMaxDelayFrame=2`

의미:
- `RIO Registered Buffer Pool` 전환 후에도 `Rio Direct`는 고부하 10분 런을 안정적으로 통과했다.

### 3-6. 최신 1시간 4모드 고부하 비교 (`2026-04-06`, no-race)
조건:
- `250 sessions`
- `holdSeconds=3600`
- `interval=0`
- `room-change=90%`
- `connectsPerSecond=10`
- `Server / Contents / Client worker = 4 / 4 / 4`
- `ContentsRuntime` 인위적 race / sleep 주입 비활성

실행 로그:
- [rio_ownerthread_250x1h_interval0](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_ownerthread_250x1h_interval0)
- [rio_direct_250x1h_interval0](D:\Project\ServerPortfolio\RefactoringServer\Out\rio_direct_250x1h_interval0)
- [iocp_default_250x1h_interval0](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_default_250x1h_interval0)
- [iocp_sendbuf0_250x1h_interval0](D:\Project\ServerPortfolio\RefactoringServer\Out\iocp_sendbuf0_250x1h_interval0)

총 응답 수:

| Mode | responses total |
| --- | ---: |
| IocpSendBufDefault | 6254918 |
| IocpSendBuf0 | 6121986 |
| Rio Direct | 6083244 |
| Rio OwnerThread | 6076360 |

steady-state 평균:

| Mode | avg sendTPS | avg CPU | avg roomPacketTPS |
| --- | ---: | ---: | ---: |
| IocpSendBufDefault | 4894.0 | 9.61% | 4894.0 |
| Rio OwnerThread | 4799.8 | 10.47% | 4799.6 |
| Rio Direct | 4681.4 | 11.11% | 4681.4 |
| IocpSendBuf0 | 4679.6 | 10.25% | 4679.6 |

공통 안정성:
- 네 모드 모두 `echo validation succeeded`
- `client/server err log` 비어 있음
- `enqueueFailTPS=0`
- `roomMaxQueue=5`
- `roomMaxDelayFrame=1`

해석:
- 최신 1시간 고부하에서는 `IOCP Default`가 가장 높은 처리량을 기록했다.
- `RIO` 두 모드는 안정성은 충분히 확보했지만 throughput은 `IOCP Default`보다 대략 `2.7% ~ 2.9%` 낮았다.
- 이번 workload는 `payloadSize=16`, `packetsPerSend=1` 중심의 작은 패킷 고빈도 전송이라 `SO_SNDBUF=-1` 이점이 살아나는 패턴으로 보인다.
- 현재 `RIO` 경로에는 여전히 send hot path 오버헤드가 남아 있다.
  - send마다 `SSendRequestContext`를 `new/delete`
  - `RIOSend`를 `1 buffer = 1 call`로 사용
  - submit 시 `requestQueueMutex` 획득
- 반면 `IOCP`는 `FillSendBatch(32)` 기반 `WSASend` batching 경로를 이미 사용한다.

### 3-7. Lock-Free TLS free-list 적용 후 10분 RTT 비교 (`2026-04-07`)
추가 변경:
- `FSendSegmentPool`의 bucket free-list를 `global mutex + vector`에서
  `TLS local free-list + lock-free shared stack` 구조로 전환했다.
- 빠른 경로는 thread-local cache에서 lock 없이 alloc/free를 처리하고,
  local cache 부족/초과 시에만 shared stack과 교환한다.

조건:
- `250 sessions`
- `holdSeconds=600`
- `interval=0`
- `room-change=90%`
- `connectsPerSecond=10`
- `Server / Contents / Client worker = 4 / 4 / 4`
- `ContentsRuntime` 인위적 race / sleep 주입 비활성
- RTT CSV 수집 활성

실행 로그:
- [iocp_default](D:\Project\ServerPortfolio\RefactoringServer\Out\rtt_compare_10m_tlsfreelist_20260406_235530\iocp_default)
- [rio_direct](D:\Project\ServerPortfolio\RefactoringServer\Out\rtt_compare_10m_tlsfreelist_20260406_235530\rio_direct)
- [rio_owner](D:\Project\ServerPortfolio\RefactoringServer\Out\rtt_compare_10m_tlsfreelist_20260406_235530\rio_owner)

총 응답 수:

| Mode | responses total |
| --- | ---: |
| Rio Direct | 740201 |
| Rio OwnerThread | 736714 |
| IOCP Default | 710564 |

steady-state 평균:

| Mode | avg sendTPS | avg CPU | avg moveTPS |
| --- | ---: | ---: | ---: |
| Rio Direct | 3381.424 | 18.778% | 1089.453 |
| Rio OwnerThread | 3374.903 | 19.214% | 1082.661 |
| IOCP Default | 3254.249 | 18.149% | 1045.777 |

RTT 요약:

| Mode | login avg | room-list avg | room-enter avg | room-change-list avg | room-change avg | echo avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| IOCP Default | 36.163ms | 48.024ms | 53.501ms | 60.520ms | 107.729ms | 58.714ms |
| Rio Direct | 29.934ms | 65.640ms | 66.128ms | 57.977ms | 103.359ms | 56.521ms |
| Rio OwnerThread | 44.581ms | 36.934ms | 43.346ms | 58.295ms | 103.922ms | 56.667ms |

공통 안정성:
- 세 모드 모두 `echo validation succeeded`
- `client/server err log` 비어 있음
- stage별 `overall_timeout_count = 0`

해석:
- `Lock-Free TLS free-list` 적용 후 이번 10분 RTT 비교에서는 `Rio Direct`가 가장 높은 총량과 `sendTPS`를 기록했다.
- steady-state 핵심 경로인 `echo-response`, `room-change`, `room-change-list` 평균 RTT도 두 RIO 모드가 `IOCP Default`보다 소폭 유리했다.
- bootstrap 구간은 성격이 갈렸다.
  - `Rio Direct`는 `login-response` 평균이 가장 좋았다.
  - `Rio OwnerThread`는 `room-list`, `room-enter` 평균이 가장 좋았다.
- tail max는 여전히 크다.
  - 예를 들어 `room-change` max는 `Rio Direct 5016.676ms`, `Rio OwnerThread 4639.207ms`, `IOCP Default 4110.266ms`였다.
  - 즉 평균 개선은 보였지만 긴 꼬리는 아직 남아 있다.
- 현재 RTT CSV는 `avg / max / timeout` 집계만 제공하고 `p95 / p99`는 아직 기록하지 않는다.

## 4. 현재 결론
- `RIO Registered Buffer Pool` 전환은 안정성 기준으로 성공이다.
- 최신 `Rio Direct` 10분 고부하와 1시간 비교 런 모두 정상 종료했다.
- `FSendSegmentPool`의 `Lock-Free TLS free-list` 전환도 기능/안정성 기준으로 성공이다.
- 최신 1시간 총량 기준 순위는 `IocpSendBufDefault > IocpSendBuf0 > Rio Direct > Rio OwnerThread`였지만,
  최신 10분 RTT 비교 기준으로는 `Rio Direct > Rio OwnerThread > IOCP Default`가 나왔다.
- 즉 현재 `RIO`는 완전히 뒤처지는 구조가 아니고, allocator contention을 줄이면 오히려 `IOCP Default`를 넘는 구간도 확인된다.
- `IOCP`는 현재 구현 기준 `SO_SNDBUF=-1` 기본값이 가장 유리하다.
- `RIO`의 다음 우선순위는 allocator 이후 단계인 `send hot path 오버헤드 감소`다.
  - `SSendRequestContext` heap alloc/free 제거
  - `RIOSend` batching
  - submit lock 계측 및 축소

## 5. 다음 작업
- `RIO send hot path` 오버헤드 감소
  - `SSendRequestContext` heap alloc/free 제거
  - `RIOSend` batching 도입
  - `requestQueueMutex` hot path 정리
- RTT 집계 확장
  - `FRttCsvLogger`에 `p95 / p99` 추가
  - 현재 `avg / max / timeout` 중심 비교를 percentile 기반으로 보강
- `broadcast packet fan-out` 경로 추가
- `IOCP` send path copy 감소 재평가
- 필요 시 서버/클라이언트 분리 머신 조건에서 재비교
