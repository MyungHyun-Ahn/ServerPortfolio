# RIO Registered Buffer Pool 전환 계획

## 1. 목적
- 현재 `RIO` send 경로의 `per-send register/deregister`를 제거한다.
- 가능한 범위에서 backend 간 send memory 모델을 정리한다.
- 장기적으로 `RIO`에서는 이 registered memory를 `session-local send ring`의 backing store로 사용한다.

## 2. 현재 상태
- `recv`는 세션 생성 시 staging buffer를 한 번 register하고 재사용하는 구조다.
- `send`는 기존에 요청마다 `RIORegisterBuffer -> RIOSend -> RIODeregisterBuffer`를 수행하는 구조였다.
- 현재 send segment pool 도입으로 이 hot path 등록/해제 비용은 많이 줄었지만, 최종 형태는 아직 아니다.

## 3. 수정된 방향

### 3-1. RIO 최종형은 packet-size bucket allocator가 아니다
- `RIO`가 최종적으로 `session-local send ring`으로 가면
  - `256`
  - `512`
  - `1024`
  - `2048`
  - `4096`
  - `8192`
같은 packet-size bucket은 `RIO` steady-state send path의 핵심 추상화가 아니다.
- 핵심은:
  - 세션마다 고정 크기의 registered region 하나를 갖고
  - 그 영역을 contiguous ring buffer처럼 쓰는 구조다.

### 3-2. bucket 기반이 남는 곳
- packet-size bucket은 여전히 다음 용도로는 의미가 있다.
  - `IOCP` send segment allocation
  - 공용 fallback send memory
  - oversized packet fallback
- 하지만 `RIO` session send ring 자체에는 필수 요소가 아니다.

## 4. 목표 구조

### 4-1. 세션별 registered ring region
- 각 `RIO` 세션은 고정 크기의 registered send region 하나를 가진다.
- 기본 크기는 `64 KiB`로 둔다.
- 이 region을 ring buffer처럼 사용한다.
- 패킷은 offset을 계산해서 그 ring에 append된다.
- flush 시에는 이 ring 내부의 contiguous 구간 하나를 `RIO_BUF`로 만들어 보낸다.

### 4-2. register 수명 모델
- send ring은 `FRioSession` 풀 객체가 생성될 때, 또는 풀 확장 시점에 register한다.
- 세션이 풀로 돌아갈 때는 deregister하지 않는다.
- 풀 반환 시에는 다음 상태만 초기화한다.
  - offset
  - in-flight 상태
  - pending flush 상태
- `RIODeregisterBuffer`는 다음 시점에만 수행한다.
  - 서버 종료
  - 또는 미래 단계의 pool shrink / destroy

즉 register 비용은 steady-state send traffic이 아니라, 풀 확장 횟수에 비례하게 만든다.

### 4-3. backend별 역할
- `RIO`
  - 세션별 고정 크기 registered region 사용
  - `BufferId + Offset + Length`
  - contiguous flush만 허용
- `IOCP`
  - 현재 send 경로를 유지
  - 필요하면 별도 pooled send buffer 또는 공용 fallback memory를 유지

## 5. Ring Buffer가 가득 찼을 때 대응
- ring이 가득 찼다고 해서 in-flight 구간을 덮어쓰거나 두 번째 send를 동시에 걸면 안 된다.
- 현재 정책은 `64 KiB` ring이 정상 크기 패킷 append를 더 이상 수용하지 못하면 이를 `send stall`로 간주한다.
- 즉 ring full은 정상적인 backpressure 상황이 아니라 비정상 상황으로 본다.

처리 규칙:
1. in-flight 바이트는 절대 overwrite하지 않는다.
2. 세션당 두 번째 `RIOSend`는 절대 걸지 않는다.
3. `8 KiB` 이하의 정상 패킷조차 ring에 더 못 들어가면 `send stall`로 판정한다.
4. `send stall` 발생 시 로그를 남기고 해당 세션을 종료하거나 fail-fast 한다.

즉 1차 구현에서는 retry/backpressure보다 비정상 탐지와 단순한 종료 정책을 우선한다.

## 6. 패킷 크기 정책
- `RIO` send ring 기준 최대 패킷 크기는 `8 KiB`로 둔다.
- 이는 `PacketGenerator`의 `MaxEncodedSizeBytes`와 같은 기준으로 맞춘다.
- `8 KiB`를 넘는 패킷은 정상 범위 밖으로 보고:
  - debug에서는 assert 또는 강한 진단
  - runtime에서는 로그 + send 실패
로 처리한다.

즉 `RIO` steady-state 경로는 다음 두 값을 고정 전제로 둔다.
- 세션당 send ring: `64 KiB`
- 최대 패킷 크기: `8 KiB`

## 7. 왜 이 구조가 단순한가
- `RIO` steady-state send path에서 packet-size bucket lookup이 사라진다.
- packet마다 slot allocator를 태우지 않아도 된다.
- 다음 구조와 잘 맞는다.
  - `OwnerThread` serialize
  - `Direct` append + 짧은 session lock
  - 세션당 단일 in-flight send

## 8. 실제 의미
- 이 문서는 이제 단순한 “registered pool” 문서가 아니다.
- 의미는 다음과 같다.
  - 단기: 현재 registered send memory 구조를 안정화
  - 다음 단계: `RIO`를 `session-local fixed-size registered ring`으로 진화
- 즉 registered buffer pool은 최종 목표가 아니라, ring 설계의 기반 메모리 모델이다.

## 9. 성공 기준
- `RIO` steady-state send path에서 per-send register/deregister 제거
- 세션별 고정 크기 registered ring region 확보
- 이후 `RIO send hot path reduction` 단계로 자연스럽게 연결

## 10. 결론
- 최종 `RIO` send 설계의 핵심은 packet-size bucket allocator가 아니다.
- 올바른 장기 모델은 `세션마다 고정 크기 registered send ring 하나`를 갖는 구조다.
