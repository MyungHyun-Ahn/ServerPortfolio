# ContentsRuntime enqueue 경로 최적화 리뷰

## 1. 목적
- hot path 분석 결과를 바탕으로 실제 contention 지점을 줄였다.
- 적용 대상은 두 가지였다.
  1. `FContentRuntime::EnqueuePacket` 경량화
  2. `FContentThread` packet inbox lock-free 프로토타입

## 2. 적용 내용

### 2.1 `FContentRuntime::EnqueuePacket` 경량화
- 파일:
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
- 변경 전:
  - `sessionId -> contentId` 조회가 `unordered_map + mutex` 기반
  - packet마다 전역 락을 거치며 라우팅 조회
- 변경 후:
  - `sessionId` 하위 비트를 slot index로 사용하는 route table 도입
  - packet hot path는 route table 직접 조회
  - runtime 락은 `shared_mutex` 기반으로 조정하고, packet enqueue는 `shared_lock` 경로 사용

### 2.2 packet inbox lock-free 프로토타입
- 파일:
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)
- 변경 전:
  - `packetQueue`가 `std::deque + mutex`
- 변경 후:
  - `FLockFreeQueue<SQueuedOwnedPacket*>` 기반 packet inbox 추가
  - queue item은 TLS pool 재사용
  - producer는 락 없이 enqueue
  - consumer는 frame loop에서 drain 후 기존 owned packet 처리 루프 재사용

## 3. 되돌리기 방법
- `packet inbox` lock-free는 프로토타입이라 쉽게 되돌릴 수 있게 유지했다.
- 토글 위치:
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)
- 스위치:
  - `kUseLockFreePacketInboxPrototype`
- `false`로 바꾸면 기존 `std::deque + mutex` 경로로 바로 복귀한다.

## 4. 수치 비교
- 변경 전
  - `runtimeEnqueueLockUs=88376.00`
  - `runtimeEnqueueMaxLockUs=2470.40`
  - `echoPacketEnqueueLockUs=64276.10`
  - `echoPacketEnqueueMaxLockUs=1699.80`
- 변경 후
  - `runtimeEnqueueLockUs=19765.10`
  - `runtimeEnqueueMaxLockUs=53.40`
  - `echoPacketEnqueueLockUs=0.00`
  - `echoPacketEnqueueMaxLockUs=0.00`

## 5. 기능 검증
- 단발 스모크 통과
  - [contents_lockfree_smoke2_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_lockfree_smoke2_client.log)
  - `echo validation succeeded.`
- 반복 스모크 통과
  - [contents_lockfree_smoke3_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_lockfree_smoke3_client.log)
  - `echo validation succeeded. sessions=1 responses=6 ... holdSeconds=1`

## 6. 장시간 안정성 결과
- 6시간 race injection 검증 통과
  - [launcher_6h_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\launcher_6h_20260403_032550.log)
  - [client_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\client_20260403_032550.log)
  - [server_20260403_032550.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents-race-validation\server_20260403_032550.log)
- 핵심 결과
  - `echo validation succeeded. sessions=100 responses=10375394 ... holdSeconds=21600`
  - 마지막 구간에서도 `enqueueFailTPS=0`
  - `echoPacketEnqueueLockUs=0.00`
  - 세션들은 정상적으로 `client disconnected`, `echo content leave`, `Session closed`로 정리됨

## 7. 결론
- 1단계 경량화의 주 효과는 `FContentRuntime::EnqueuePacket`였다.
- 2단계 lock-free 프로토타입은 packet inbox 경합을 사실상 제거했다.
- 현재 기준으로는
  - route table 기반 runtime enqueue
  - lock-free packet inbox prototype
  조합이 충분히 안정적으로 보인다.
- 다만 완전한 최종 결론이라기보다, 현재 프로젝트 단계에서 실사용 가능한 수준으로 판단할 수 있다.
