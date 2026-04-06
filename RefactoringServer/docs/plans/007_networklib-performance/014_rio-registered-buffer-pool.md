# RIO Registered Buffer Pool 전환 계획

## 1. 목적
- 현재 `FRioServer` send path는 패킷마다 `RIORegisterBuffer -> RIOSend -> RIODeregisterBuffer`를 수행한다.
- 이를 `공용 offset 기반 send segment pool` 구조로 바꾸고, `RIO`일 때만 region에 register metadata를 붙이는 방향으로 전환한다.
- 목표는 `RIO` send hot path의 등록/해제 오버헤드를 제거하고, 장기적으로 `IOCP`와 `RIO`가 같은 send memory 모델을 공유하게 만드는 것이다.

## 2. 현재 상태
### 2-1. recv
- recv는 세션 생성 시 staging buffer를 `RIORegisterBuffer` 한 번만 수행하고 재사용한다.
- 즉 recv 쪽은 이미 `pre-registered buffer` 구조에 가깝다.

### 2-2. send
- send는 현재 요청마다 `FPacketBuffer` 메모리를 직접 `RIORegisterBuffer` 한다.
- send completion 시 `RIODeregisterBuffer`를 수행한다.
- 즉 현재 send는 `registered buffer pool`이 아니라 `per-send register/deregister` 구조다.

### 2-3. 현재 문제
- `RIORegisterBuffer / RIODeregisterBuffer` 호출이 send hot path에 들어간다.
- 작은 payload가 자주 나갈수록 등록/해제 오버헤드 비중이 커진다.
- 현재 `FPacketBuffer`, `FSendBuffer`의 page reuse는 `std::vector` capacity 재사용일 뿐, offset 기반 slice allocator는 아니다.

## 3. 목표 구조
- 공용 send memory pool은 `64 KiB 고정 region`을 여러 개 들고 있는 `offset 기반 segment pool`로 만든다.
- 각 region은 같은 버킷의 고정 크기 slot으로 나뉜다.
- `IOCP`
  - 공용 pool에서 받은 slice를 그대로 `WSABUF`로 사용한다.
- `RIO`
  - 같은 region을 미리 `RIORegisterBuffer` 하고, send 시 `BufferId + Offset + Length`만 사용한다.
- send completion이 오면 slice를 pool에 반환한다.
- 즉 `메모리 풀은 공용`, `RIO register metadata는 backend 전용`으로 분리한다.

## 4. 설계 방향
### 4-1. 공용 구성요소
- 새 구성요소 예시:
  - `FSendSegmentPool`
  - `FSendSegmentRegion`
  - `FSendSegmentSlice`
  - `FRioRegisteredRegionMetadata`
- region 공통 메타:
  - `char* base`
  - `std::size_t capacity`
  - `bucketClass`
  - `slotSize`
  - `slotCount`
- `RIO` backend인 경우에만 region마다
  - `RIO_BUFFERID bufferId`
를 추가로 가진다.

### 4-2. send slice 단위
- send 1건은 region 전체를 쓰지 않고 `slice` 하나만 점유한다.
- `IOCP`
  - `WSABUF.buf = region.base + slice.offset`
  - `WSABUF.len = actual length`
- `RIO`
  - `RIO_BUF.BufferId = region.bufferId`
  - `RIO_BUF.Offset = slice.offset`
  - `RIO_BUF.Length = actual length`
로 구성한다.

### 4-3. 복사 전략
- 현재 `FPacketBuffer` payload는 일반 메모리에 있으므로, send slice로 1회 복사해야 한다.
- 1차 구조는
  - `PacketBuffer -> Send Segment Slice` 1회 복사
  - backend별 send submit
로 간다.
- 1차 목표는 `register/deregister 제거`와 `공용 send memory 모델` 도입이다.
- `copy elimination`은 후속 최적화로 본다.

### 4-4. owner-thread / direct 공용화
- `RioSendDispatchMode: Direct | OwnerThread` 여부와 무관하게 실제 submit 경로는 동일한 slice 기반 send 함수를 사용한다.
- 차이는 `누가 submit을 호출하느냐`만 다르게 둔다.
- 장기적으로 `IOCP`도 같은 공용 send segment pool을 사용하도록 맞춘다.

## 5. 세부 구조
### 5-1. region 크기
- region/page 크기는 `64 KiB`로 고정한다.
- 이유:
  - region lifetime 관리가 단순하다.
  - bucket별 slot 개수 계산이 쉽다.
  - RIO register metadata 추적이 단순하다.
  - 디버깅과 통계 수집이 쉽다.

### 5-2. slot allocator
- `고정 크기 slot allocator`를 채택한다.
- 가변 allocator는 1차에서 도입하지 않는다.
- size bucket 예시:
  - `256`
  - `512`
  - `1024`
  - `2048`
  - `4096`
  - `8192`
- 각 bucket은 자기 전용 `64 KiB region`들을 가진다.
- 각 region은 같은 size class slot만 포함한다.
- 예:
  - `256B bucket`의 `64 KiB region`은 `256`개 slot
  - `1024B bucket`의 `64 KiB region`은 `64`개 slot
  - `4096B bucket`의 `64 KiB region`은 `16`개 slot

### 5-3. contention 완화
- 1차는 `TLS 메모리 풀처럼 버킷 기반`으로 간다.
- 핵심은 `같은 bucket 안에서만 alloc/free`가 일어나게 하는 것이다.
- 1차 구조:
  - bucket별 free-list
  - bucket별 region 목록
  - bucket별 thread-safe alloc/free
- 즉 `global pool + per-region lock`이 아니라, `bucket 중심 구조`를 기본으로 둔다.
- `worker-local free list`는 2차 최적화로 보류한다.
- 이유:
  - completion이 다른 worker에서 올 수 있으므로, 1차는 remote free correctness를 먼저 단순하게 보장해야 한다.
  - 따라서 1차는 `bucket별 global free-list` 또는 `bucket별 striped free-list`를 사용한다.
- 같은 bucket이라면 다른 free-list로 반환돼도 correctness상 문제 없도록 설계한다.

### 5-4. send request context
- 현재 `SSendRequestContext`는 `packetBuffer`, `bufferId`, `RIO_BUF`를 가진다.
- 전환 후에는
  - `packetBuffer`
  - `bucketClass`
  - `regionIndex`
  - `slotIndex`
  - `sliceOffset`
  - backend별 send view(`WSABUF` 또는 `RIO_BUF`)
를 들고 completion 시 slice를 반환한다.
- `RIO` completion에서는 `RIODeregisterBuffer`를 더 이상 호출하지 않는다.

## 6. API / 설정 계획
### 6-1. 설정
- 예시:
  - `UseSendSegmentPool: true | false`
  - `SendSegmentRegionSizeBytes = 65536`
  - `SendSegmentRegionCountPerBucket`
  - `SendSegmentBucketClassBytes`
  - `RioRegisterSendSegments: true | false`

### 6-2. rollout
- 1차는 기존 경로를 남겨둔다.
- 설정 또는 내부 enum으로
  - `LegacyVectorSendBuffer`
  - `SendSegmentPool`
를 선택 가능하게 두고 A/B 검증한다.

## 7. PacketGenerator 연계
### 7-1. 기본 방향
- bucket 선택 기준은 `최종 encoded size`다.
- packet schema / generated serializer 단계에서 `최대 encoded size` 규칙을 같이 가져간다.

### 7-2. packet size 메타 규칙
- packet 정의 파일에 `PacketSize` 또는 `MaxEncodedSizeBytes`를 명시할 수 있게 한다.
- packet이 전부 고정 길이 필드로만 구성된 경우:
  - 사용자가 size를 입력하지 않아도 generator가 자동 계산한다.
- packet에 가변 길이 필드가 하나라도 포함된 경우:
  - 예: `string`, `vector`, `bytes`, 가변 배열
  - 사용자가 최대 size를 명시하지 않으면 generator 단계에서 오류를 발생시킨다.

### 7-3. generator 결과
- generated packet/type에는 아래 메타를 같이 만든다.
  - `static constexpr std::size_t kFixedEncodedSizeBytes`
  - 또는
  - `static constexpr std::size_t kMaxEncodedSizeBytes`
- 고정 길이 packet은 `kFixedEncodedSizeBytes == kMaxEncodedSizeBytes`로 취급한다.
- 가변 길이 packet은 반드시 `kMaxEncodedSizeBytes`가 존재해야 한다.

### 7-4. serializer 단계 검증
- 검사는 enqueue 직전이 아니라 `serializer/build` 단계에서 수행한다.
- 흐름:
1. packet serialize/build
2. 실제 encoded size 계산
3. `actualEncodedSize <= kMaxEncodedSizeBytes` 검증
4. 통과 시 bucket 선택
- 검증 실패 시:
  - debug에서는 assert 또는 강한 진단
  - runtime에서는 로그 + send enqueue 실패 반환

### 7-5. bucket 선택
- serializer가 최종 encoded size를 반환하면,
- send segment pool은 그 값을 기준으로 가장 가까운 상위 bucket을 선택한다.
- 예:
  - `180B -> 256B`
  - `900B -> 1024B`
  - `3000B -> 4096B`
- `kMaxEncodedSizeBytes`가 가장 큰 pool bucket보다 크면:
  - 별도 large packet 정책
  - 또는 기존 경로 fallback
중 하나를 선택해야 한다.

## 8. observability
- 서버 통계에 아래 항목 추가
  - `sendSegmentRegionCount`
  - `sendSegmentBytesTotal`
  - `sendSegmentBytesInUse`
  - `sendSegmentAllocFailCount`
  - `sendSegmentFallbackCount`
  - `sendSegmentPoolUsagePercent`
  - `rioRegisteredRegionCount`
- 로그
  - region 등록 실패
  - slice 부족
  - fallback 사용

## 9. 실패 시 동작
- 1차는 send slice를 못 구하면 두 선택지가 있다.
1. 기존 `per-send register` 경로로 fallback
2. send 실패로 처리

- 1차 rollout은 fallback을 유지하는 쪽이 안전하다.
- 이후 안정화되면 fallback 제거 여부를 다시 판단한다.

## 10. 구현 단계
### 10-1. 1단계
- 공용 `FSendSegmentPool` 추가
- `64 KiB` region/page allocator 구현
- bucket별 고정 slot allocator 구현
- backend 공통 send memory 모델 도입
- `RIO`일 때만 region allocate + `RIORegisterBuffer`

### 10-2. 2단계
- send slice allocate / release 구현
- `SSendRequestContext`를 slice 반환형으로 변경
- `SubmitSendDirect()`를 `SendSegmentPool` 경로로 연결

### 10-3. 3단계
- `OwnerThread` 경로도 같은 slice allocator 사용
- stats/log 추가
- `IOCP`도 같은 공용 send segment slice를 사용하도록 맞춘다.

### 10-4. 4단계
- `PacketGenerator` size 메타 추가
- generator의 자동 계산 / 명시 강제 규칙 추가
- serializer 단계 size 검증 연결

### 10-5. 5단계
- A/B 벤치마크
  - `LegacyVectorSendBuffer`
  - `SendSegmentPool`
- 조건:
  - `Direct`
  - `OwnerThread`
  - `interval=0`
  - `room-change=10%`
  - 최신 `ContentsRuntime` 기준

## 11. 검증 항목
- correctness
  - send 누락 없음
  - completion 후 slice 이중 반환 없음
  - session close 중 completion 정리 정상
  - schema에 가변 길이 필드가 있는데 최대 size 미명시 시 generator가 실패하는지 확인
  - serializer 단계 size 초과 검증이 정상 동작하는지 확인
- 안정성
  - 장시간 soak에서 alloc fail/fallback 패턴 확인
  - `RIO` region 등록/해제 수명 정상
  - `IOCP`와 `RIO`가 같은 공용 slice allocator 위에서 정상 동작하는지 확인
- 성능
  - `avg sendTPS`
  - `avg sendBps`
  - CPU
  - tail RTT

## 12. 위험 요소
- 공용 region lifetime 관리가 잘못되면 use-after-free가 된다.
- slice 재사용 타이밍이 completion보다 빠르면 데이터 오염이 난다.
- bucket class가 payload 분포와 안 맞으면 내부 낭비가 커질 수 있다.
- fallback 경로와 pool 경로를 동시에 두는 동안 분기 복잡도가 올라간다.
- packet schema의 size 메타와 실제 serializer 결과가 어긋나면 false positive / false negative 검사가 생길 수 있다.

## 13. 결론
- 현재 `RIO` send는 `RegisteredBufferPool` 구조가 아니고, `IOCP`/`RIO` 모두 offset 기반 공용 send memory 모델도 없다.
- 다음 단계는 `64 KiB region + bucket별 고정 slot allocator` 기반의 공용 `send segment pool`을 도입하고, `RIO`일 때만 해당 region에 register metadata를 붙이는 것이다.
- 1차 목표는 `공용 offset 기반 send memory 모델 + RIO register/deregister 제거`, 2차 목표는 `bucket contention과 copy 비용 튜닝`이다.
