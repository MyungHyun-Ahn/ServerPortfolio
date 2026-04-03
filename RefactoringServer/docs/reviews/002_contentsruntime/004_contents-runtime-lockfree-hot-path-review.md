# ContentsRuntime lock-free hot path 리뷰

## 1. 목적
- `ContentsRuntime` 전체를 바로 lock-free로 바꾸지 않고, 실제 contention이 큰 경로를 먼저 찾는다.
- `packet inbox`를 선택한 이유를 계측 결과로 설명한다.

## 2. 측정 방법
- 서버: `OutTempHot\\EchoServer.exe --headless`
- 클라이언트:
  - `--sessions 100`
  - `--count 8`
  - `--response-thread-count 1`
  - `--responses-per-thread 1`
  - `--hold-seconds 5`
  - `--interval-ms 0`
  - `--packets-per-send 4`
  - `--quiet`
- 로그:
  - [contents_hotpath_server.log](D:\Project\ServerPortfolio\RefactoringServer\Out\contents_hotpath_server.log)

## 3. 계측 항목
- `FContentRuntime`
  - `EnterSession`, `LeaveSession`, `EnqueuePacket`, `MoveSession` 호출 수
  - 누적 lock wait, 최대 lock wait
- `FContentThread`
  - `EnqueueEnter`, `EnqueueLeave`, `EnqueuePacket` 호출 수
  - 누적 lock wait, 최대 lock wait

## 4. 측정 결과
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
- 가장 유력한 hot path는 예상대로 `packet enqueue`였다.
- 그중에서도 1차 병목 후보는 `FContentRuntime::EnqueuePacket`의 라우팅 조회 경로였다.
- 이유:
  - 호출 수가 압도적으로 많다.
  - 누적 lock wait가 `MoveSession`, `EnterSession`, `LeaveSession`보다 훨씬 크다.
  - 콘텐츠 스레드 내부 packet enqueue lock wait도 크지만, runtime 라우팅 lock wait이 더 앞단에서 커졌다.

## 6. 우선순위 판단
1. `FContentRuntime::EnqueuePacket`
2. `FContentThread::EnqueuePacket`
3. `MoveSession`, `EnterSession`, `LeaveSession`

## 7. 결론
- `packet inbox`를 lock-free 1차 대상으로 고른 판단은 타당했다.
- lifecycle 계열 함수는 중요하지만 hot path 우선순위는 낮았다.
- 따라서 이후 작업은
  - runtime enqueue 경량화
  - packet inbox lock-free 프로토타입
  순서로 진행하는 것이 맞다.
