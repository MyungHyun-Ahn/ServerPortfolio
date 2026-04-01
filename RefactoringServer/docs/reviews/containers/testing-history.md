# Lock-Free Containers Testing History

## 1. 문서 역할
- 이 문서는 `FLockFreeQueue`, `FLockFreeStack` 관련 테스트 계획과 실행 이력을 한 곳에 모아 둔 기록 문서다.
- 현재 구조 판단과 설계 근거는 [lock-free-containers-review.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\containers\lock-free-containers-review.md)를 우선한다.

## 2. 테스트 대상
- [`FLockFreeQueue.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\Containers\FLockFreeQueue.h)
- [`FLockFreeStack.h`](D:\Project\ServerPortfolio\RefactoringServer\NetworkLib\Include\NetworkLib\Containers\FLockFreeStack.h)
- [`LockFreeTests/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\LockFreeTests\Main.cpp)
- [`LockFreeQueueSoakTest/Main.cpp`](D:\Project\ServerPortfolio\RefactoringServer\LockFreeQueueSoakTest\Main.cpp)

## 3. 기본 검증 항목
- `Queue linear FIFO`
  - 단일 스레드에서 1~1000 enqueue/dequeue 순서가 유지되는지 확인한다.
- `Queue parallel sum`
  - 4 producer, 4 consumer 기준으로 총 개수와 합계가 일치하는지 확인한다.
- `Stack parallel sum`
  - 4 producer, 4 consumer 기준으로 총 개수와 합계가 일치하는지 확인한다.

## 4. soak 테스트 항목
- `LockFreeQueueSoakTest`
  - `--seconds`, `--producers`, `--consumers`, `--report-seconds`, `--log-path`를 받아 장시간 생산/소비 무결성을 확인한다.
- 합격 기준
  - 종료 코드가 0이다.
  - 최종 `produced == consumed`이다.
  - 최종 `producedSum == consumedSum`이다.
  - 마지막 줄이 `PASS`다.

## 5. 실행 기록
### 5-1. 2026-04-01 기본 기능 테스트
- `RefactoringServer.sln` x64 Debug 빌드 성공
- [`LockFreeTests.exe`](D:\Project\ServerPortfolio\RefactoringServer\Out\LockFreeTests.exe)
  - `Queue linear FIFO : PASS`
  - `Queue parallel sum : PASS`
  - `Stack parallel sum : PASS`

### 5-2. 2026-04-01 짧은 soak 테스트
- 로그: [`queue_sanity.log`](D:\Project\ServerPortfolio\GameServer\Out\queue_sanity.log)
- 확인 결과
  - 최종 `produced == consumed`
  - 최종 `producedSum == consumedSum`
  - 마지막 줄 `PASS`

### 5-3. 2026-04-01 장시간 soak 테스트
- 로그: [`queue_overnight.log`](D:\Project\ServerPortfolio\GameServer\Out\queue_overnight.log)
- 확인 결과
  - 최종 `produced = 60423450219`
  - 최종 `consumed = 60423450219`
  - 최종 `producedSum = 17715748990661240722`
  - 최종 `consumedSum = 17715748990661240722`
  - 마지막 줄 `PASS`

## 6. 현재 해석
- 현재 테스트 범위에서는 queue와 stack 모두 즉시 드러나는 무결성 오류를 보이지 않았다.
- 이후 컨테이너 구조를 수정하면 먼저 `LockFreeTests`, 그다음 `LockFreeQueueSoakTest`를 같은 기준으로 다시 실행한다.
