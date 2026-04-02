# ContentsRuntime 락프리 hot path 리뷰

## 1. 목적
- `ContentsRuntime` 전체를 바로 락프리화하기 전에 실제 contention이 큰 구간을 계측으로 확인한다.
- 감으로 `packet inbox`를 고르지 않고, 누적 호출 수와 누적/최대 lock wait를 기준으로 우선순위를 정한다.

## 2. 측정 방법
- 서버: `OutTempHot\EchoServer.exe --headless`
- 클라이언트: `Out\EchoClient.exe --sessions 100 --count 8 --response-thread-count 1 --responses-per-thread 1 --hold-seconds 5 --interval-ms 0 --packets-per-send 4 --quiet`
- 서버 로그: [contents_hotpath_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_hotpath_server.log)

## 3. 계측 항목
- `FContentRuntime`
  - `EnterSession`, `LeaveSession`, `EnqueuePacket`, `MoveSession`의 호출 수
  - 각 경로의 누적 lock wait, 최대 lock wait
- `FContentThread`
  - `EnqueueEnter`, `EnqueueLeave`, `EnqueuePacket`의 호출 수
  - 각 경로의 누적 lock wait, 최대 lock wait

## 4. 실측 결과
- 부하 구간에서 가장 큰 수치는 아래와 같았다.
- `runtime enqueue`
  - `enqueueCalls=196776`
  - `runtimeEnqueueLockUs=88376.00`
  - `runtimeEnqueueMaxLockUs=2470.40`
- `echo content packet enqueue`
  - `echoPacketEnqueueCalls=196676`
  - `echoPacketEnqueueLockUs=64276.10`
  - `echoPacketEnqueueMaxLockUs=1699.80`
- `move`
  - `runtimeMoveLockUs=99.40`
  - `runtimeMoveMaxLockUs=56.40`
- `auth content packet enqueue`
  - `authPacketEnqueueCalls=100`
  - `authPacketEnqueueLockUs=16.50`
  - `authPacketEnqueueMaxLockUs=0.60`

## 5. 해석
- 실제 hot path는 예상대로 `packet enqueue`였다.
- 그중에서도 1차 병목 후보는 `FContentRuntime::EnqueuePacket`의 전역 락이다.
- 이유:
  - 호출 수가 압도적으로 많다.
  - 누적 lock wait가 `echo content` 내부 packet queue lock보다 더 크다.
  - `MoveSession`, `EnterSession`, `LeaveSession`은 정합성 구간이면서 누적 대기 시간이 매우 작다.
- 따라서 지금 단계에서 락프리화를 검토할 우선순위는 아래와 같다.
  1. `FContentRuntime::EnqueuePacket`
  2. `FContentThread::EnqueuePacket`
  3. 그 외 `MoveSession`, `EnterSession`, `LeaveSession`은 유지

## 6. 권장 다음 단계
- `sessionId -> contentId` 조회와 `content thread enqueue`를 한 번에 처리하는 경로를 다시 본다.
- 1차 구현은 `packet inbox`만 락프리화하는 보수적 접근이 적절하다.
- `MoveSession`과 lifecycle 경로는 정합성 때문에 당분간 락 유지가 맞다.

## 7. 주의점
- 이번 측정은 실제 contention 위치를 찾는 용도다.
- 절대 시간 자체보다 `어느 경로가 상대적으로 많이 잠기느냐`를 보는 데 의미가 있다.
- `OutTempHot` 임시 빌드로 측정했으므로, 최종 후보 적용 후에는 일반 빌드 기준으로 다시 확인이 필요하다.
