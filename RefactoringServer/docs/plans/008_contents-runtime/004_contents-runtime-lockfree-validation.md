# ContentsRuntime 락프리 inbox 안정성 검증 계획

## 1. 목적
- `packet inbox` 락프리 프로토타입이 기능적으로 안전한지 확인한다.
- contention 감소만이 아니라 종료, 세션 정리, 반복 송수신, 장시간 동작까지 같이 본다.
- 필요하면 즉시 기존 `deque + mutex` 경로로 되돌릴 수 있도록 A/B 비교 기준을 남긴다.

## 2. 검증 대상
- `FContentThread` packet inbox 락프리 프로토타입
- 토글 위치:
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentThread.cpp)
- 토글 값:
  - `kUseLockFreePacketInboxPrototype = true`
- rollback 방법:
  - 위 값을 `false`로 바꾸면 기존 `deque + mutex` 경로로 복귀

## 3. race injection 검증 원칙
- 이번 안정성 검증에서는 경합을 줄이는 것이 아니라, 숨어 있는 경쟁 상태를 더 잘 드러내는 것이 목적이다.
- 따라서 `Sleep(0)`, `SwitchToThread()`, `std::this_thread::yield()` 같은 API를
  - 성능 최적화 기법이 아니라
  - 테스트 전용 race window 확장 도구
  로 사용한다.
- 적용 원칙:
  - `Debug` 또는 테스트 전용 토글에서만 활성화
  - 운영 경로에는 넣지 않음
  - 실패 시 즉시 끌 수 있어야 함

### 3.1 권장 방식
- 별도 `RaceInjection` 토글을 둔다.
- 무조건 매번 호출하지 않고 아래 방식 중 하나로 삽입한다.
  - 확률 기반: 예) 5%, 10%
  - 주기 기반: 예) 100회 중 1회
- 우선순위 API:
  1. `SwitchToThread()`
  2. `Sleep(0)`
  3. `std::this_thread::yield()`

### 3.2 삽입 후보 지점
- `FContentRuntime::EnqueuePacket`
  - route lookup 직전/직후
  - `targetThread` 확보 직후
- `FContentRuntime::MoveSession`
  - source/target 결정 직후
  - route 갱신 직전/직후
- `FContentThread::EnqueuePacket`
  - lock-free queue enqueue 직전/직후
- `FContentThread` consumer loop
  - drain 직전/직후

## 4. 검증 단계

### 4.1 기본 기능 검증
- 목적:
  - 최소 경로가 깨지지 않았는지 확인
- 시나리오:
  - `sessions=1`
  - `count=1`
  - `holdSeconds=0`
- 기대 결과:
  - `echo validation succeeded.`
  - `login -> chat snapshot -> echo -> disconnect` 순서 정상

### 4.2 반복 경로 검증
- 목적:
  - 같은 연결 유지 중 반복 송수신이 멈추지 않는지 확인
- 시나리오:
  - `sessions=1`
  - `count=1`
  - `holdSeconds=1`
  - `intervalMs=200`
- 기대 결과:
  - 반복 구간에서 `EchoRq/EchoRp`만 계속 처리
  - 세션 종료 시 `echo content leave`, `client disconnected` 정상 출력

### 4.3 다중 세션 검증
- 목적:
  - 여러 producer가 동시에 packet inbox에 넣을 때 꼬임이 없는지 확인
- 시나리오:
  - `sessions=8`
  - `count=2`
  - `holdSeconds=1`
  - `intervalMs=200`
  - `packetsPerSend=2`
- 기대 결과:
  - 응답 누락 없음
  - 종료 후 세션 수 0으로 복귀

### 4.4 고부하 contention 검증
- 목적:
  - 실제 contention 감소가 유지되는지 확인
- 시나리오:
  - `sessions=100`
  - `count=4~8`
  - `holdSeconds=3~5`
  - `intervalMs=0`
  - `packetsPerSend=4`
- 기대 결과:
  - `runtimeEnqueueLockUs`가 기존보다 낮음
  - `echoPacketEnqueueLockUs`가 크게 줄거나 0에 가까움
  - `enqueueFailTPS=0`

### 4.5 race injection 검증
- 목적:
  - 일부러 스레드 교차 타이밍을 넓혀 숨은 경쟁 상태를 찾는다.
- 시나리오:
  - `sessions=8~100`
  - `holdSeconds=1~10`
  - race injection 토글 활성화
  - `SwitchToThread()` 또는 `Sleep(0)`를 삽입한 빌드 사용
- 기대 결과:
  - deadlock 없음
  - packet 유실 없음
  - 세션 종료 불능 없음
  - `enqueueFailTPS=0`

### 4.6 장시간 안정성 검증
- 목적:
  - short run에서 안 보이는 queue 누수/종료 불능/세션 잔존 여부 확인
- 시나리오:
  - `sessions=50~100`
  - `holdSeconds=7200`
  - 장시간 반복 송수신
- 기대 결과:
  - 세션 수가 비정상적으로 남지 않음
  - 종료 후 프로세스가 정상 정리됨
  - 로그에 `enqueueFailTPS` 증가 없음

## 5. A/B 비교 기준
- A안: `kUseLockFreePacketInboxPrototype = false`
- B안: `kUseLockFreePacketInboxPrototype = true`
- C안: `kUseLockFreePacketInboxPrototype = true` + race injection on

비교 항목:
- `runtimeEnqueueLockUs`
- `runtimeEnqueueMaxLockUs`
- `echoPacketEnqueueLockUs`
- `echoPacketEnqueueMaxLockUs`
- `enqueueFailTPS`
- `echoQueue`, `echoMaxQueue`
- 세션 종료 여부

## 6. 실패 기준
- `echo validation succeeded.` 미출력
- 세션이 종료 후에도 남음
- `enqueueFailTPS > 0`
- `client disconnected` 이후 `echo content leave` 누락
- 장시간 테스트에서 프로세스 종료 불능 또는 queue depth 비정상 증가

## 7. 최종 판단 기준
- 기능 검증, 반복 검증, 다중 세션 검증을 모두 통과하면 프로토타입 유지 가능
- 고부하에서 contention 감소가 분명하고 기능 회귀가 없으면 다음 단계 확정
- race injection 검증에서 문제 발생 시 원인 추적 전까지 운영 기본값에는 반영하지 않음
- 장시간 테스트에서 문제 발생 시 즉시 토글을 `false`로 되돌리고 원인 추적

## 8. TODO
- 일반 `Out\\EchoServer.exe` 기준으로도 같은 검증 다시 수행
- 장시간 테스트 결과를 별도 review 문서에 누적
