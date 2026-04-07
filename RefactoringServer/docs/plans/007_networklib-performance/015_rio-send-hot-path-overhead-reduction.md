# RIO Send Hot Path 오버헤드 감소 계획

## 1. 목적
- registered buffer pool과 TLS free-list 적용 이후에도 남아 있는 `RIO` send hot path 오버헤드를 줄인다.
- `RIO`를 `session-local send ring` 기반 구조로 바꾼다.
- `IOCP` 경로는 그대로 유지한다.

## 2. 설계 판단

### 2-1. scatter-gather는 기본 전략으로 채택하지 않는다
- 이번 단계의 기본 전략은 `N packet -> N RIO_BUF -> 1 RIOSend`가 아니다.
- 실제 환경에서도 `MaxSendDataBuffers > 1` 설정 시 `WSAEINVAL(10022)`가 발생했다.
- 그와 별개로도 더 견고한 구조는 세션별 contiguous send region 하나를 쓰는 방식이다.

### 2-2. 대신 `N packet -> 1 contiguous send buffer -> 1 RIOSend`로 간다
- 여러 패킷을 세션별 registered send ring에 연속으로 적재한다.
- flush 시점에는 contiguous 구간 하나를 `RIO_BUF` 하나로 만들어 `RIOSend` 1회로 보낸다.
- 즉 batching은 scatter-gather가 아니라 `session-local contiguous aggregation`으로 해결한다.

## 3. 핵심 모델

### 3-1. 세션당 하나의 고정 크기 send ring
- 각 `RIO` 세션은 큰 registered send region 하나를 가진다.
- 기본 크기는 `64 KiB`로 둔다.
- 그 region을 ring buffer처럼 사용한다.
- steady-state send path에서는 packet-size bucket allocator가 핵심이 아니다.
- 핵심 상태는 다음이다.
  - `capacity`
  - `base pointer`
  - `RIO_BUFFERID`
  - `writeOffset`
  - `flushOffset`
  - `inFlightOffset`
  - `inFlightLength`
  - `pendingFlush`

### 3-2. 세션당 in-flight send는 항상 1개
- 세션당 동시에 outstanding `RIOSend`는 최대 1개만 허용한다.
- 이미 send가 in-flight면:
  - 두 번째 `RIOSend`를 걸지 않고
  - `pending` 상태만 기록한다.
- 다음 send는 completion 이후에만 시작한다.

이 규칙이 correctness의 핵심이다.

### 3-3. 최대 패킷 크기
- `RIO` steady-state 경로의 최대 패킷 크기는 `8 KiB`로 둔다.
- 이 기준은 `PacketGenerator`의 `MaxEncodedSizeBytes`와 맞춘다.
- `8 KiB`를 넘는 패킷은 fallback으로 우회하지 않고 비정상 상황으로 처리한다.

## 4. 모드별 계획

### 4-1. `RIO OwnerThread`
- send request는 owner worker queue에 enqueue만 한다.
- 실제 수행은 owner thread가 한다.
  - serialization
  - cipher encode
  - framing
  - ring append
  - flush
- 같은 세션의 send ring을 owner thread 하나만 만지므로 session send lock이 필요 없다.

### 4-2. `RIO Direct`
- 호출 스레드가 직접 serialize와 ring append를 수행한다.
- 여러 producer가 같은 세션으로 동시에 들어올 수 있으므로 session send lock이 필요하다.
- 이 lock은 다음만 보호하는 것을 목표로 한다.
  - 공간 확인
  - offset 예약
  - flush 상태 갱신

## 5. 직렬화 전략

### 5-1. RIO 경로
- `FRioServer::SendPacket(...)`가 통합 지점이 된다.
- steady-state RIO 경로에서는 패킷마다 별도 `FSendBuffer`를 만들지 않는 것이 목표다.
- 대신 reserved ring 구간에 바로 packet을 쓴다.

### 5-2. IOCP 경로
- `FIocpServer::SendPacket(...)`는 현재 경로를 유지한다.
- 이번 단계에서는 IOCP 설계를 밀어 바꾸지 않는다.

## 6. Ring 규칙
- in-flight 구간은 절대 overwrite하지 않는다.
- 한 번의 `RIOSend`는 항상 ring 내부 contiguous 구간 하나에 대응한다.
- tail에 다음 패킷이 안 들어가면:
  - head로 wrap한다
  - 단, in-flight 구간과 겹치면 안 된다
- `8 KiB`를 넘는 oversized packet은 steady-state ring 경로에 넣지 않는다.
- 이 경우는 비정상으로 보고 실패 처리한다.

## 7. Ring이 가득 찼을 때 정책
- ring이 가득 찼다고 해서:
  - in-flight 구간을 덮어쓰거나
  - 세션당 두 번째 send를 동시에 걸면 안 된다

기본 정책:
1. send in-flight가 없으면 flush 가능한 구간을 먼저 보낸다
2. send in-flight가 있더라도 정상 크기 패킷 append 공간이 더 이상 없으면 `send stall`로 본다
3. `send stall`은 비정상 상황으로 간주하고 세션 종료 또는 fail-fast 처리한다

즉 1차 구현은 correctness 우선:
- overwrite 금지
- 세션당 in-flight send 1개 유지
- ring full 상태를 backpressure로 숨기지 않음
- 정상 패킷 append 불가 시 즉시 비정상 처리

## 8. Flush 규칙
- 다음 조건 중 하나면 flush한다.
  - 다음 패킷을 넣을 공간이 부족함
  - queue drain이 끝남
  - latency threshold 도달
  - high-priority packet
  - completion 시 `pendingFlush` 존재

1차는 단순하게 간다.
- append 후
- send in-flight가 없고
- contiguous flushable bytes가 있으면
- `RIOSend` 1회만 건다

## 9. 구현 단계

### Phase A. 정리
- scatter-gather 실험 흔적 제거
- `MaxSendDataBuffers = 1` 유지
- 현재 RIO batching 실험 코드를 안정적인 baseline으로 되돌린다

### Phase B. OwnerThread 우선
- `FRioSession`에 send ring 상태 추가
- owner-thread queue payload를 `send request` 중심으로 바꾼다
- owner thread가 serialize + append + flush를 모두 수행한다

### Phase C. Direct 확장
- 같은 send ring 모델을 재사용한다
- session lock을 추가한다
- 세션당 in-flight send 1개 규칙은 그대로 유지한다

### Phase D. 계측
- 아래 지표를 추가한다.
  - `rioSendRingBytesInUse`
  - `rioSendRingFlushCount`
  - `rioSendRingAvgFlushBytes`
  - `rioSendRingPendingFlushCount`
  - `rioSendRingWrapCount`
  - `rioSendLockUs`
  - `rioSendLockMaxUs`

## 10. 성공 기준
- `RIO OwnerThread`, `RIO Direct` 모두 correctness 유지
- 세션당 send in-flight 최대 1개 유지
- `IOCP` 동작과 성능 회귀 없음
- steady-state `RIO` 경로에서 packet당 `FSendBuffer` 생성 제거 또는 대폭 축소
- `64 KiB` ring, `8 KiB` max packet 정책 아래 장시간 런 안정성 확인
- ring full 발생 시 `send stall` 로그와 비정상 처리 경로가 명확히 동작
- 장시간 soak에서 `RIO` throughput 또는 RTT 개선 확인

## 11. 결론
- 최종 `RIO` send 설계의 핵심은 scatter-gather나 packet-size bucket이 아니다.
- 올바른 모델은:
  - `세션당 고정 크기 registered send ring 1개`
  - `그 ring에 직접 serialize`
  - `contiguous 구간 하나를 flush`
  - `세션당 in-flight send 1개 유지`
  - `ring full은 send stall로 보고 비정상 처리`
이다.
