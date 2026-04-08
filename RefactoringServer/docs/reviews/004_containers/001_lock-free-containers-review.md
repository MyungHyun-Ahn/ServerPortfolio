# Lock-Free Containers Review

## 0. 문서 역할
- 이 문서는 `lock-free queue`, `lock-free stack`, 컨테이너 공통 테스트에 대한 현재 기준 문서다.
- 상세 테스트 실행 이력은 [testing-history.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\004_containers\002_testing-history.md)에 분리해 둔다.
- 아래 문서들의 공통 주제를 이 문서로 통합했다.
  - `networklib-lockfree-containers-bootstrap.md`
  - `networklib-lockfree-structure-simplification.md`
  - `networklib-lockfree-stress-review.md`
  - `networklib-soak-tests-review.md` 중 queue 관련 부분
- 기존 `mmorpg-v2` 레거시 문서는 이 문서와 테스트 이력 문서로 흡수했다.

## 1. 범위
- 대상 코드
  - [`FLockFreeQueue.h`](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Include\NetworkLib\Containers\FLockFreeQueue.h)
  - [`FLockFreeStack.h`](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Include\NetworkLib\Containers\FLockFreeStack.h)
  - [`LockFreeCommon.h`](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Include\NetworkLib\Containers\LockFreeCommon.h)
- 대상 테스트
  - [`LockFreeTests/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\SmokeTests\LockFreeTests\Main.cpp)
  - [`LockFreeQueueSoakTest/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\SmokeTests\LockFreeQueueSoakTest\Main.cpp)

## 2. 현재 구조와 방향
- 기존 포트폴리오 자산을 새 `RefactoringServer/NetworkLib` 경로로 이관했다.
- `FLockFreeQueue`는 인스턴스 상태를 `head`, `tail` 두 포인터만 가지는 구조로 단순화했다.
- `FLockFreeStack`는 인스턴스 상태를 `top` 포인터만 가지는 구조로 단순화했다.
- `size`, `useCount` 같은 보조 카운터는 컨테이너의 필수 상태로 보지 않고 제거했다.
- 컨테이너 사용 코드는 `empty 여부` 자체보다 `Dequeue()/Pop()` 성공 여부 중심으로 작성한다.

## 3. 판단 근거
### 3-1. 핵심 정합성은 포인터 CAS에 있다
- queue는 `head`, `tail`, node의 `next`가 핵심 상태다.
- stack은 `top`, node의 `next`가 핵심 상태다.
- `size/useCount`는 알고리즘 성립에 필수 상태가 아니고, 보조 관측값에 가깝다.

### 3-2. lock-free 제어 흐름에서는 size가 안전한 판단 기준이 아니다
- 다른 스레드가 동시에 상태를 바꾸므로 `if (size > 0)` 같은 분기는 의미가 약하다.
- 실제 제어 흐름은 `if (queue.Dequeue(value))`, `if (stack.Pop(value))`처럼 시도 결과로 판단해야 한다.

### 3-3. 불필요한 경쟁 지점을 줄인다
- 보조 카운터를 매 연산마다 갱신하면 추가 contention이 생긴다.
- 현재 프로젝트의 목표가 `Interlocked` 중심 최소 상태 코어이므로, 핵심 상태 외 멤버를 줄이는 쪽이 방향과 맞다.

## 4. 현재 코드 사실
### 4-1. `FLockFreeQueue`
- dummy node 기반 Michael-Scott queue 형태를 사용한다.
- `Dequeue()`는 `head->next == nullptr`일 때 비었다고 판단한다.
- 구조 검증 근거
  - [`LockFreeTests/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\SmokeTests\LockFreeTests\Main.cpp)의 `static_assert(sizeof(TQueue) == sizeof(std::int64_t) * 2)`

### 4-2. `FLockFreeStack`
- tagged pointer 기반 단일 top 스택 구조다.
- `Pop()`은 `m_top == nullptr`이면 false를 반환한다.
- 구조 검증 근거
  - [`LockFreeTests/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\SmokeTests\LockFreeTests\Main.cpp)의 `static_assert(sizeof(TStack) == sizeof(std::int64_t))`

### 4-3. `LockFreeCommon`
- tagged pointer 조합과 64비트 CAS 래퍼를 공통으로 제공한다.
- queue/stack이 같은 pointer-tag 규칙을 공유하도록 정리했다.

## 5. 검증 근거
### 5-1. 빌드 및 초기 통합
- `RefactoringServer/RefactoringServer.sln` x64 Debug 빌드 성공
- `LockFreeSmoke.cpp`를 통해 컨테이너 헤더가 실제 라이브러리 빌드 경로를 타도록 구성했다.

### 5-2. 기능 테스트
- [`LockFreeTests.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
  - `Queue linear FIFO : PASS`
  - `Queue parallel sum : PASS`
  - `Stack parallel sum : PASS`

### 5-3. 짧은 soak 테스트
- [`queue_sanity.log`](D:\Project\ServerPortfolio\GameServer\Out\queue_sanity.log)
  - 최종 `produced == consumed`
  - 최종 `producedSum == consumedSum`
  - 마지막 줄 `PASS`

### 5-4. 장시간 soak 테스트
- [`queue_overnight.log`](D:\Project\ServerPortfolio\GameServer\Out\queue_overnight.log)
  - 최종 `produced == consumed`
  - 최종 `producedSum == consumedSum`
  - 마지막 줄 `PASS`

## 6. 현재 판단
- 컨테이너의 핵심 상태는 현재 수준에서 충분히 단순화됐다.
- 현재 테스트 범위에서는 queue와 stack 모두 즉시 드러나는 무결성 문제를 보이지 않았다.
- lock-free 컨테이너는 앞으로도 `Pop()/Dequeue()` 성공 여부 중심으로 사용하는 것이 맞다.

## 7. 남은 리스크
- 현재 검증은 무결성 중심이다. 성능 우위 자체를 증명한 것은 아니다.
- 메모리 회수는 pool 정책에 기대고 있으므로, 향후 reclamation 전략을 더 강화할 여지는 있다.
- queue/stack이 실제 네트워크 hot path에 들어갔을 때의 contention 양상은 별도 측정이 필요하다.

## 8. 후속 작업
- 실제 송신 큐 또는 세션 이벤트 큐에 `FLockFreeQueue` 적용 검토
- queue/stack 벤치마크 추가
- `approx size`가 필요한지 여부는 컨테이너가 아니라 관측 계층 기준으로 재검토



