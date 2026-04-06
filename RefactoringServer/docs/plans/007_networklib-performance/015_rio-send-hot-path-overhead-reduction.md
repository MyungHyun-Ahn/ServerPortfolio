# RIO Send Hot Path Overhead Reduction

## 1. 목적
- `RIO Registered Buffer Pool` 전환 이후에도 최신 1시간 고부하 비교에서 `RIO`가 `IOCP Default`보다 약간 낮게 나왔다.
- 현재 병목은 correctness가 아니라 `send hot path`의 남은 오버헤드로 본다.
- 목표는 `RIO send` 경로에서 불필요한 heap alloc/free, 작은 단위 submit, submit lock 비용을 줄여 throughput을 끌어올리는 것이다.

## 2. 현재 관찰
### 2-1. 최신 비교 결과 (`2026-04-06`)
조건:
- `250 sessions`
- `holdSeconds=3600`
- `interval=0`
- `room-change=90%`
- `connectsPerSecond=10`
- `Server / Contents / Client worker = 4 / 4 / 4`
- `ContentsRuntime` 인위적 race / sleep 주입 비활성

결과:

| Mode | responses total | steady avg sendTPS | steady avg CPU |
| --- | ---: | ---: | ---: |
| IocpSendBufDefault | 6254918 | 4894.0 | 9.61% |
| IocpSendBuf0 | 6121986 | 4679.6 | 10.25% |
| Rio Direct | 6083244 | 4681.4 | 11.11% |
| Rio OwnerThread | 6076360 | 4799.8 | 10.47% |

해석:
- `RIO`는 안정성은 확보했지만, 작은 패킷 고빈도 workload에서 아직 `IOCP Default`를 넘지 못했다.
- 다음 단계는 `RIO send hot path` 자체를 가볍게 만드는 것이다.

### 2-2. 추가 관찰: TLS free-list allocator 적용 후 재비교 (`2026-04-07`)
추가 변경:
- `FSendSegmentPool`의 bucket free-list를 `TLS local free-list + lock-free shared stack`으로 전환했다.

10분 RTT 비교 결과:

| Mode | responses total | avg sendTPS | avg CPU | echo avg RTT |
| --- | ---: | ---: | ---: | ---: |
| Rio Direct | 740201 | 3381.424 | 18.778% | 56.521ms |
| Rio OwnerThread | 736714 | 3374.903 | 19.214% | 56.667ms |
| IOCP Default | 710564 | 3254.249 | 18.149% | 58.714ms |

해석:
- send segment allocator contention은 실제 병목 후보였고, local cache를 넣자 `RIO`가 다시 앞서는 구간이 확인됐다.
- 즉 allocator 층은 한 차례 의미 있게 개선된 상태다.
- 이제 남은 주 병목은 `SSendRequestContext` 할당/반환, `RIOSend` 호출 수, submit lock 쪽으로 더 좁혀진다.

### 2-3. 현재 코드에서 보이는 오버헤드 후보
1. send마다 `SSendRequestContext`를 heap에 할당/해제
   - `FRioServer.cpp`의 `SubmitSendDirect()`에서 `new FRioSession::SSendRequestContext{}`
   - completion에서 `delete &requestContext`

2. `RIOSend`가 `1 buffer = 1 call`
   - 현재 `RIO_BUF` 하나만 넘겨서 submit
   - `IOCP`는 `FillSendBatch(32)` 기반 `WSASend` batching 경로를 이미 사용

3. submit 시 `requestQueueMutex` 획득
   - `FRioSession`이 request queue 보호용 mutex를 들고 있음
   - 현재 `Direct` 경로는 send hot path에서 이 mutex를 잡는다

4. `OwnerThread` 모드는 queue hop이 추가됨
   - owner worker inbox enqueue / drain이 한 번 더 들어간다
   - 다만 최신 결과에서는 `OwnerThread`와 `Direct` 차이가 크지 않아서, 1차 우선순위는 아니다

## 3. 목표
### 3-1. 단기 목표
- send hot path에서 heap alloc/free 제거
- send submit당 처리하는 buffer 수 증가
- submit lock/critical section 비용 축소
- 이미 개선된 항목:
  - `FSendSegmentPool` allocator의 TLS local cache / lock-free shared stack 전환

### 3-2. 비목표
- 이번 단계에서 send copy elimination까지 같이 하지 않는다
- `Registered Buffer Pool` 구조 자체를 다시 뒤집지 않는다
- `IOCP` send 모델 변경은 부가 비교 대상으로만 둔다

## 4. 구현 방향
### 4-1. 1단계: `SSendRequestContext` pool/slab
- `FRioSession` 또는 worker-local context slab에 `SSendRequestContext`를 미리 확보한다
- send submit은 `new/delete` 대신 free-list pop/push를 사용한다
- completion이 끝나면 context를 slab로 반환한다

권장 형태:
- `FRioSession::AcquireSendRequestContext()`
- `FRioSession::ReleaseSendRequestContext()`
- session당 fixed-capacity slab 또는 lock-free stack

이유:
- 현재 가장 확실한 hot path overhead다
- 구현 리스크가 비교적 작다

### 4-2. 2단계: `RIOSend` batching
- `IOCP`의 `FillSendBatch(32)`와 비슷한 개념을 `RIO`에도 넣는다
- 여러 `FSendBuffer`를 모아 `RIO_BUF[]`를 구성한 뒤 `RIOSend` 1회로 묶어 보낸다
- `Registered Buffer Pool`과도 자연스럽게 연결된다

필요 변경:
- session send queue에서 여러 send buffer를 뽑아 batch 구성
- `SSendRequestContext`가 단일 buffer가 아니라 batch metadata를 들도록 확장
- completion에서 batch 전체 slice/context를 반환

### 4-3. 3단계: submit lock 정리
- 현재 `requestQueueMutex`는 correctness를 위해 hot path에 들어와 있다
- 1차 목표는 mutex 제거보다 `잠금 범위 축소`다

후보:
1. `session closing + request queue lifetime`을 더 명확히 고정하고 normal submit에서 mutex를 제거
2. 또는 `owner-thread only submit`로 직렬화하고 direct 경로를 단순화
3. 또는 session ref/closing 상태만으로 submit 가능성을 판정하고 RQ pointer를 안정 lifetime으로 승격

우선순위:
- 먼저 1단계, 2단계로 성능 차이를 확인
- lock 정리는 그 다음

## 5. 세부 단계
### 5-1. Phase A: context pool
- `SSendRequestContext`를 pool/slab 기반으로 전환
- 기존 API shape는 최대한 유지
- `RIO Direct`, `RIO OwnerThread` 공통 경로 적용

### 5-2. Phase B: batch submit
- `FRioSession`에 `FillSendBatchForRio(maxCount)` 추가
- `RIO_BUF[]`와 `FSendBuffer*[]`를 request context에 보관
- `RIOSend` call 수 대비 실제 전송 packet 수 비율 측정

### 5-3. Phase C: lock instrumentation
- `requestQueueMutex` 획득 시간 측정
- `rioSendSubmitLockUs`, `rioSendSubmitMaxLockUs` 같은 지표 추가
- lock이 진짜 병목인지 먼저 계측

### 5-4. Phase D: 필요 시 lock 정리
- instrumentation 결과가 의미 있게 크면 구조 변경
- 아니면 batching과 context pool까지만으로도 충분할 수 있다

## 6. 계측
추가할 지표:
- `rioSendContextPoolMissCount`
- `rioSendContextPoolInUse`
- `rioSendSubmitCalls`
- `rioSendBuffersSubmitted`
- `rioSendAvgBatchSize`
- `rioSendSubmitLockUs`
- `rioSendSubmitMaxLockUs`
- `rioSendFallbackRegisterCount`

## 7. 검증 계획
1. 기능 검증
- `RIO Direct`, `RIO OwnerThread` 짧은 스모크
- send completion 누락, context 반환 누락, slice 반환 누락 확인

2. 안정성 검증
- `250 sessions`
- `interval=0`
- `room-change=90%`
- `10분`

3. 성능 검증
- `RIO OwnerThread`, `RIO Direct`, `IOCP Default`, `IOCP Buffer 0`
- 같은 조건 `1시간`
- 비교값:
  - `responses total`
  - `avg sendTPS`
  - `avg CPU`
  - `avg roomPacketTPS`
  - `avg batch size`

## 8. 성공 기준
- `RIO` 두 모드 모두 기존 안정성 유지
- `FSendSegmentPool` TLS free-list 적용 후 얻은 이득을 regress 없이 유지
- `send hot path`에서 heap alloc/free 제거 확인
- `avg batch size > 1` 달성
- 최신 동일 조건 기준 `RIO Direct`가 적어도 현재 baseline보다 의미 있게 개선
- 가능하면 `IOCP Default`와 차이를 줄이거나 역전

## 9. 결론
- `Registered Buffer Pool` 다음 단계는 `send hot path overhead reduction`이다.
- allocator local cache 단계는 선행 반영됐고, 이제 남은 핵심은 context / batching / submit lock이다.
- 구현 우선순위는
  1. `SSendRequestContext` pool
  2. `RIOSend` batching
  3. submit lock 계측 및 정리
순으로 잡는 것이 가장 안전하다.
