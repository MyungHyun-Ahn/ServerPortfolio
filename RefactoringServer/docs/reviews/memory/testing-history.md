# Memory Pool Testing History

## 1. 문서 역할
- 이 문서는 `FLockFreeMemoryPool`, `FTlsMemoryPoolManager` 관련 테스트 계획과 실행 이력을 한 곳에 모아 둔 기록 문서다.
- 현재 구조 판단과 설계 근거는 [memory-pool-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\memory\memory-pool-review.md)를 우선한다.

## 2. 테스트 대상
- [`FLockFreeMemoryPool.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\Memory\FLockFreeMemoryPool.h)
- [`FTlsMemoryPool.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\Memory\FTlsMemoryPool.h)
- [`LockFreeTests/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\Main.cpp)
- [`TlsMemoryPoolSoakTest/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\TlsMemoryPoolSoakTest\Main.cpp)

## 3. 기본 검증 항목
- `TLS memory pool parallel`
  - 8 thread 기준으로 batch alloc/free 반복 후 `GetUseCount() == 0` 복귀를 확인한다.

## 4. soak 테스트 항목
- `TlsMemoryPoolSoakTest`
  - `--seconds`, `--threads`, `--batch-size`, `--report-seconds`, `--log-path`를 받아 장시간 alloc/free 무결성을 확인한다.
- 합격 기준
  - 종료 코드가 0이다.
  - 최종 `alloc == free`다.
  - 최종 `inUse == 0`이다.
  - 마지막 줄이 `PASS`다.

## 5. 실행 기록
### 5-1. 2026-04-01 기본 기능 테스트
- `RefactoringServer.sln` x64 Debug 빌드 성공
- [`LockFreeTests.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
  - `TLS memory pool parallel : PASS`
  - 종료 시 `GetUseCount() == 0`

### 5-2. 2026-04-01 짧은 soak 테스트
- 로그: [`tls_sanity.log`](D:\Project\ServerPortfolio\GameServer\Out\tls_sanity.log)
- 확인 결과
  - 최종 `alloc == free`
  - 최종 `inUse == 0`
  - 마지막 줄 `PASS`

### 5-3. 2026-04-01 장시간 soak 테스트
- 로그: [`tls_overnight.log`](D:\Project\ServerPortfolio\GameServer\Out\tls_overnight.log)
- 확인 결과
  - 최종 `alloc = 124479256832`
  - 최종 `free = 124479256832`
  - 최종 `inUse = 0`
  - 최종 `capacity = 1024`
  - 마지막 줄 `PASS`

## 6. 현재 해석
- 현재 테스트 범위에서는 TLS local cache와 shared pool 조합에서 명백한 누수 징후를 보이지 않았다.
- 이후 `BucketSize`, `BucketCount`, `UseQueue` 정책을 바꾸면 먼저 `LockFreeTests`, 그다음 `TlsMemoryPoolSoakTest`를 같은 기준으로 다시 실행한다.
