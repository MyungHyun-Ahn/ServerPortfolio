# Memory Pool Review

## 0. 문서 역할
- 이 문서는 `FLockFreeMemoryPool`, `FTlsMemoryPoolManager`, 메모리 풀 테스트와 soak 결과에 대한 현재 기준 문서다.
- 상세 테스트 실행 이력은 [testing-history.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\003_memory\002_testing-history.md)에 분리해 둔다.
- 아래 문서들의 공통 주제를 이 문서로 통합했다.
  - `networklib-lockfree-containers-bootstrap.md` 중 memory pool 관련 부분
  - `networklib-tls-memory-pool-refactoring.md`
  - `networklib-lockfree-stress-review.md` 중 TLS memory pool 관련 부분
  - `networklib-soak-tests-review.md` 중 TLS memory pool 관련 부분
- 기존 `mmorpg-v2` 레거시 문서는 이 문서와 테스트 이력 문서로 흡수했다.

## 1. 범위
- 대상 코드
  - [`FLockFreeMemoryPool.h`](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Include\NetworkLib\Memory\FLockFreeMemoryPool.h)
  - [`FTlsMemoryPool.h`](D:\Project\ServerPortfolio\RefactoringServer\Libraries\NetworkLib\Include\NetworkLib\Memory\FTlsMemoryPool.h)
- 대상 테스트
  - [`LockFreeTests/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\SmokeTests\LockFreeTests\Main.cpp)
  - [`TlsMemoryPoolSoakTest/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\SmokeTests\TlsMemoryPoolSoakTest\Main.cpp)

## 2. `FLockFreeMemoryPool` 판단
### 2-1. 역할
- shared backing pool 역할을 맡는다.
- 핵심 상태는 free-list top 하나이며, `capacity`, `useCount`는 알고리즘 필수값이 아니라 관측용 메타데이터다.

### 2-2. 현재 유지 이유
- `capacity`
  - 실제 노드 생성량을 관찰하는 데 유용하다.
- `useCount`
  - 테스트 종료 시 outstanding allocation이 남았는지 빠르게 확인하는 데 유용하다.
- 따라서 현재는 최소 상태 철학을 해치지 않는 범위의 관측성 도구로 유지한다.

## 3. `FTlsMemoryPoolManager` 리팩터링 요약
### 3-1. 이전 문제
- 이름은 TLS memory pool이었지만, 실질적으로는 shared pool wrapper 수준에 가까웠다.
- thread-local cache 이점이 부족했고, 스레드 종료 시 cache 정리 경로도 약했다.

### 3-2. 현재 구조
- shared backing pool
  - `FLockFreeMemoryPool<SPoolNode>`
- thread-local cache
  - `SLocalCache`
  - `freeList`
  - `cachedCount`
- alloc
  - local cache가 비면 shared pool에서 `BucketSize`만큼 refill
- free
  - local cache에 우선 반납
  - `BucketSize * BucketCount`를 넘으면 trim
- thread 종료
  - `FlsAlloc` 콜백에서 local cache flush 후 shared pool로 반납

### 3-3. 왜 `FlsAlloc`로 바꿨는가
- `TlsAlloc`만으로는 스레드 종료 시 사용자 정리 콜백이 없다.
- 이번 구현은 정상 종료 시 local cache에 남은 노드를 shared pool로 되돌리기 위해 `FlsAlloc` 기반으로 바꿨다.
- 이로 인해 "프로세스 종료 시 OS가 다 정리하니 상관없다" 수준이 아니라, 정상 종료 경로에서도 pool 메모리를 정리하는 방향이 생겼다.

## 4. 검증 근거
### 4-1. 빌드 및 초기 통합
- `RefactoringServer/RefactoringServer.sln` x64 Debug 빌드 성공
- `NetworkLib`와 `LockFreeTests` 경로에서 메모리 풀 헤더가 실제 컴파일 경로를 통과했다.

### 4-2. 병렬 기능 테스트
- [`LockFreeTests.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
  - `TLS memory pool parallel : PASS`
  - 종료 시 `GetUseCount() == 0`

### 4-3. 짧은 soak 테스트
- [`tls_sanity.log`](D:\Project\ServerPortfolio\GameServer\Out\tls_sanity.log)
  - 최종 `alloc == free`
  - 최종 `inUse == 0`
  - 마지막 줄 `PASS`

### 4-4. 장시간 soak 테스트
- [`tls_overnight.log`](D:\Project\ServerPortfolio\GameServer\Out\tls_overnight.log)
  - 최종 `alloc == free`
  - 최종 `inUse == 0`
  - `capacity = 1024`
  - 마지막 줄 `PASS`

## 5. 현재 판단
- `FLockFreeMemoryPool`은 shared pool로서 충분히 단순하다.
- `FTlsMemoryPoolManager`는 이제 실제 TLS cache를 사용한다고 봐도 된다.
- 장시간 테스트 기준에서도 alloc/free 일치와 `inUse=0` 복귀를 확인했으므로, 적어도 현재 테스트 조건에서는 명백한 누수 징후가 없다.

## 6. 남은 리스크
- 현재 `BucketSize`, `BucketCount` 정책은 경험 기반 기본값일 뿐, 측정 기반 최적값은 아니다.
- `UseQueue` 옵션은 현재 구현에서 정책 분기까지 완전히 쓰고 있지는 않다. 향후 shared cache 정책을 더 세분화할 때 다시 반영할 수 있다.
- Windows 전용 `FlsAlloc`에 의존하므로 플랫폼 독립성은 낮다.

## 7. 후속 작업
- `BucketSize`, `BucketCount`에 따른 성능 비교 측정
- hot path 타입별로 별도 pool 정책이 필요한지 검토
- 정상 종료 시 outstanding allocation이 남아 있으면 경고를 남기는 shutdown 검증 경로 추가



