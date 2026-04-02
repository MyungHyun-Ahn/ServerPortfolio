# ContentsRuntime lock-free inbox 안정성 검증 계획

## 1. 목적
- `packet inbox` lock-free 프로토타입이 실제로 안정적인지 단계적으로 검증한다.
- contention 감소만 보는 것이 아니라, 패킷 유실, 세션 종료 누락, 장시간 soak 안정성까지 함께 본다.

## 2. 검증 대상
- `FContentThread`의 lock-free packet inbox 경로
- 토글 위치
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentThread.cpp)
- 현재 토글
  - `kUseLockFreePacketInboxPrototype`

## 3. 토글 정책
- 실패 시 즉시 기존 `deque + mutex` 경로로 되돌릴 수 있어야 한다.
- 비교 기준
  - A: `lock-free = false`
  - B: `lock-free = true`
  - C: `lock-free = true` + race injection on
- 상세 정책은 [006_contents-runtime-lockfree-toggle-policy.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\006_contents-runtime-lockfree-toggle-policy.md)를 기준으로 맞춘다.

## 4. race injection 검증 정책
- 이번 검증에서 `Sleep(0)`, `SwitchToThread()`, `yield()`는 성능 최적화가 아니라 race window 확장 도구다.
- 목적은 평소에는 잘 드러나지 않는 경쟁 상태를 더 쉽게 재현하는 것이다.

### 4.1 권장 방식
- `Debug` 또는 검증 전용 경로에서만 활성화
- 무조건 매번 넣지 말고 주기 기반 또는 확률 기반으로 삽입
- 우선순위
  1. `SwitchToThread()`
  2. `Sleep(0)`
  3. `std::this_thread::yield()`

### 4.2 삽입 후보
- `FContentRuntime::EnqueuePacket`
- `FContentRuntime::MoveSession`
- `FContentThread::EnqueuePacket`
- `FContentThread` consumer drain loop

## 5. 검증 단계

### 5.1 기본 기능 검증
- 목적
  - 최소 경로가 깨지지 않는지 확인
- 시나리오
  - `sessions=1`
  - `holdSeconds=0`
- 기대
  - `echo validation succeeded.`

### 5.2 반복 경로 검증
- 목적
  - 같은 연결 유지 중 packet inbox가 멈추지 않는지 확인
- 시나리오
  - `sessions=1`
  - `holdSeconds=1`
  - `intervalMs=200`
- 기대
  - `EchoRq/EchoRp` 반복 처리
  - 정상 종료

### 5.3 다중 세션 검증
- 목적
  - 여러 producer가 동시에 enqueue할 때 유실이 없는지 확인
- 시나리오
  - `sessions=8~100`
  - `packetsPerSend=2~4`
- 기대
  - 응답 유실 없음
  - 세션 누수 없음

### 5.4 contention 검증
- 목적
  - 실제로 lock wait가 줄었는지 확인
- 시나리오
  - `sessions=100`
  - `holdSeconds=3~5`
  - `intervalMs=0`
  - `packetsPerSend=4`
- 확인 항목
  - `runtimeEnqueueLockUs`
  - `echoPacketEnqueueLockUs`
  - `enqueueFailTPS`

### 5.5 race injection 검증
- 목적
  - 숨어 있는 경쟁 상태를 드러낸다
- 시나리오
  - `sessions=8~100`
  - race injection on
- 기대
  - deadlock 없음
  - packet 유실 없음
  - 세션 종료 누락 없음

### 5.6 장시간 검증
- 목적
  - soak 안정성 확인
- 시나리오
  - `DurationSeconds=7200`
  - `sessions=100`
  - race injection on
- 기대
  - 세션 누수 없음
  - 비정상 종료 없음
  - `enqueueFailTPS=0`

## 6. 실패 기준
- `echo validation succeeded.` 미출력
- `enqueueFailTPS > 0`
- 세션 종료 누락
- 프로세스가 시간 내 정상 종료하지 못함
- bootstrap 단계 timeout

## 7. 최종 판단 기준
- 기본/반복/다중 세션 검증 통과
- contention 지표 개선
- race injection 검증 통과
- 장시간 soak 검증 통과

## 8. TODO
- 일반 `Out\\EchoServer.exe` 기준 장시간 결과 별도 문서화
- 필요하면 `2 hour`와 `8 hour`를 분리 운영
