# Lock-Free Foundation

## 1. 범위
- `FLockFreeQueue`
- `FLockFreeStack`
- `FLockFreeMemoryPool`
- `FTlsMemoryPoolManager`

## 2. 설계 방향
- 핵심 자료구조는 가능한 한 `최소 상태`만 유지한다.
- queue는 `head`, `tail`만 인스턴스 상태로 가진다.
- stack은 `top`만 인스턴스 상태로 가진다.
- `size`, `useCount` 같은 값은 알고리즘 필수 상태가 아니라면 제거하거나 관측용으로만 둔다.

## 3. 현재 구현 판단
### 3-1. queue
- Michael-Scott queue 형태를 따른다.
- 제어 흐름 판단은 `Dequeue()` 성공 여부 중심으로 본다.

### 3-2. stack
- tagged pointer 기반 top-only 구조다.
- 제어 흐름 판단은 `Pop()` 성공 여부 중심으로 본다.

### 3-3. shared memory pool
- 핵심 상태는 free-list top이다.
- `capacity`, `useCount`는 현재 관측용 메타데이터로 유지한다.

### 3-4. TLS memory pool
- shared pool 위에 thread-local cache를 얹는다.
- `FlsAlloc` 기반으로 스레드 종료 시 local cache flush 경로를 둔다.

## 4. 검증 방향
- 기본 병렬 검증은 `LockFreeTests`에서 한다.
- 장시간 무결성 검증은 `LockFreeQueueSoakTest`, `TlsMemoryPoolSoakTest`에서 한다.
- 상세 결과와 테스트 이력은 `docs/reviews` 아래 문서를 기준으로 본다.

## 5. 다음 작업
- 실제 `NetworkLib` 송신 큐 후보로 queue 적용 검토
- TLS pool 정책별 성능 비교
- 필요 시 메모리 회수 전략 강화 검토
