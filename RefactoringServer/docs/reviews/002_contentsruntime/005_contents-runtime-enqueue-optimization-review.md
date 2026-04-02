# ContentsRuntime enqueue 경로 최적화 리뷰

## 1. 목적
- `ContentsRuntime` hot path 분석 결과를 바탕으로 실제 contention이 큰 두 지점을 손본다.
- 대상은 아래 두 가지다.
  1. `FContentRuntime::EnqueuePacket` 경로 경량화
  2. `FContentThread` packet inbox 락프리 프로토타입

## 2. 적용 내용

### 2.1 `FContentRuntime::EnqueuePacket` 경량화
- 파일:
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
- 변경 전:
  - `sessionId -> contentId` 조회에 `unordered_map + mutex`
  - packet마다 전역 락을 잡고 세션 맵과 콘텐츠 맵을 순차 조회
- 변경 후:
  - `sessionId`의 하위 32비트를 slot index로 사용
  - `session route table`을 직접 조회
  - `contentSlots`는 유지하되 packet hot path는 route table 기준으로 빠르게 라우팅
  - runtime 락은 `mutex`에서 `shared_mutex`로 바꾸고, packet enqueue는 `shared_lock` 경로 사용

### 2.2 packet inbox 락프리 프로토타입
- 파일:
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)
- 변경 전:
  - `packetQueue`가 `std::deque + mutex`
- 변경 후:
  - `FLockFreeQueue<SQueuedOwnedPacket*>` 기반 packet inbox 프로토타입 추가
  - queue item은 TLS pool로 재사용
  - producer는 락 없이 enqueue
  - consumer는 frame loop에서 drain 후 기존 owned packet 처리 루프 사용

## 3. 되돌리기 방법
- 이번 락프리 inbox는 프로토타입이라 되돌리기 쉽게 유지했다.
- 토글 위치:
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)
- 스위치:
  - `kUseLockFreePacketInboxPrototype`
- 이 값을 `false`로 바꾸면 기존 `std::deque + mutex` packet inbox 경로로 바로 복귀한다.

## 4. 실측 결과

### 4.1 hot path 측정 조건
- 서버: `OutTempHot\\EchoServer.exe --headless`
- 클라이언트:
  - `--sessions 100`
  - `--count 4`
  - `--response-thread-count 1`
  - `--responses-per-thread 1`
  - `--hold-seconds 3`
  - `--interval-ms 0`
  - `--packets-per-send 4`
- 로그:
  - 변경 전: [contents_hotpath_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_hotpath_server.log)
  - 변경 후: [contents_hotpath_server_after.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_hotpath_server_after.log)

### 4.2 수치 비교
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

## 5. 해석
- `FContentRuntime::EnqueuePacket` 경량화만으로도 runtime 쪽 누적 lock wait가 크게 줄었다.
- `packet inbox`를 락프리 프로토타입으로 바꾼 뒤에는 `FContentThread::EnqueuePacket` lock wait가 0으로 떨어졌다.
- 즉 기존 hot path 두 곳 모두에서 contention 감소는 분명하게 확인됐다.

## 6. 검증 결과

### 6.1 기능 검증 통과
- 단발 스모크 통과
  - [contents_lockfree_smoke2_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_lockfree_smoke2_client.log)
  - `echo validation succeeded. sessions=1 responses=1 ...`
- 반복 스모크 통과
  - [contents_lockfree_smoke3_client.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_lockfree_smoke3_client.log)
  - `echo validation succeeded. sessions=1 responses=6 ... holdSeconds=1`

### 6.2 주의점
- 높은 backlog를 만드는 100세션 고부하 시나리오는 hot path 수집 용도로는 충분했지만, 종료 시간까지 포함한 안정성 검증으로는 쓰지 않았다.
- 따라서 이번 결론은
  - contention 감소 확인: 가능
  - 장시간/고부하 안정성 확정: 보류
  로 보는 것이 맞다.

## 7. 결론
- 1단계 경량화 대상은 예상대로 `FContentRuntime::EnqueuePacket`이었다.
- 2단계 프로토타입은 `packet inbox`에만 제한적으로 적용했고, 쉽게 되돌릴 수 있게 유지했다.
- 현재 기준으로는
  - route table 기반 runtime enqueue
  - lock-free packet inbox prototype
  조합이 가장 효과가 좋다.

## 8. TODO
- 동일 조건 장시간 테스트에서 `lock-free packet inbox` 안정성 재검증
- 필요 시 `enter/leave`도 이벤트 통합 대상으로 볼지 검토
- `OutTempHot` 임시 빌드가 아닌 일반 실행 경로 기준 재측정
