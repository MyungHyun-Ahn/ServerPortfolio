# ContentsRuntime lock-free hot path 분석 계획

## 1. 목적
- `ContentsRuntime` 전체를 무리하게 lock-free로 바꾸기 전에, 실제 병목이 어디인지 먼저 찾는다.
- 경로를 `hot path`와 `non-hot path`로 나눠 우선순위를 정한다.
- 이후 최적화는 전면 lock-free가 아니라, 효과가 큰 경로부터 단계적으로 경량화한다.

## 2. 전제
- 현재 `ContentsRuntime`는 `std::mutex + std::condition_variable + std::deque` 기반으로 시작했다.
- 구조 검증과 기능 검증은 이미 통과했고, `Login -> Chat snapshot -> Echo` 경로도 동작한다.
- 지금 단계의 질문은
  - 어디가 가장 자주 호출되는가
  - 어디가 contention을 만드는가
  - 어디를 먼저 lock-free로 바꾸는 것이 안전한가
  를 분리해서 보는 것이다.

## 3. 분석 대상
### 3.1 콘텐츠 스레드 inbox enqueue
- 대상 코드
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)
  - `EnqueueEnter`
  - `EnqueueLeave`
  - `EnqueuePacket`
- 판단
  - 이 중 `EnqueuePacket`이 가장 유력한 hot path다.
  - recv packet마다 호출되므로 호출 빈도가 가장 높다.

### 3.2 콘텐츠 스레드 consumer loop
- 대상 코드
  - `wait_until`
  - queue drain
  - packet 처리 루프
- 판단
  - producer는 여러 개, consumer는 하나여서 MPSC 패턴과 잘 맞는다.
  - packet inbox만 lock-free로 바꿔도 효과가 있을 가능성이 크다.

### 3.3 `sessionId -> contentId` 매핑
- 대상 코드
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `EnterSession`
  - `LeaveSession`
  - `EnqueuePacket`
  - `MoveSession`
- 판단
  - `EnqueuePacket`마다 조회가 일어나므로 read-heavy 경로다.
  - 다만 `MoveSession`, disconnect와의 정합성이 강하므로 전면 lock-free보다
    - slot array
    - shard
    - direct lookup
    같은 중간 단계가 더 현실적이다.

### 3.4 콘텐츠 이동 경로
- 대상 코드
  - `MoveSession`
- 판단
  - 중요하지만 packet ingress보다 호출 빈도는 낮다.
  - 정합성이 우선이므로 지금은 hot path보다 다음 단계 후보로 본다.

### 3.5 Start / Stop / RegisterContent
- 대상 코드
  - `RegisterContent`
  - `Start`
  - `Stop`
- 판단
  - 운영 중 hot path가 아니므로 lock-free 우선순위에서 제외한다.

## 4. 우선순위
### 4.1 1순위
- `FContentRuntime::EnqueuePacket`
- `FContentThread::EnqueuePacket`

### 4.2 2순위
- `sessionId -> contentId` 조회 경량화

### 4.3 3순위
- `MoveSession`
- `Stop`
- `RegisterContent`

## 5. 추천 구현 방향
### 5.1 packet queue부터 교체
- 가장 보수적인 접근
- `enter/leave`는 기존 구조 유지
- `packetQueue`만 lock-free MPSC queue로 교체

### 5.2 이벤트 통합은 후순위
- `Enter`, `Leave`, `Packet`을 하나의 이벤트 큐로 합칠 수는 있다.
- 하지만 지금은 먼저 `packetQueue` 단독 효과를 보는 편이 좋다.

### 5.3 session mapping은 즉시 전면 lock-free로 가지 않음
- read-heavy 경로지만 정합성 리스크가 크다.
- 먼저 queue 경량화 효과를 본 뒤 판단한다.

## 6. 계측 기준
- 다음 지표를 기준으로 본다.
  - `enqueueFailTPS`
  - `runtimeEnqueueLockUs`
  - `runtimeEnqueueMaxLockUs`
  - `echoPacketEnqueueLockUs`
  - `echoPacketEnqueueMaxLockUs`
  - `echoQueue`
  - `echoMaxQueue`
  - `echoLastDelayFrame`
  - `echoMaxDelayFrame`

## 7. 실행 단계
1. 현재 구조로 baseline 계측
2. `EnqueuePacket`이 실제 hot path인지 수치 확인
3. `packetQueue`만 lock-free MPSC 프로토타입 적용
4. 단기/반복/장시간 검증
5. 필요 시 session mapping 경량화 검토

## 8. 결론
- 현재 단계에서 `ContentsRuntime` 전체 lock-free는 과하다.
- 가장 유력한 hot path는 `packet inbox`이며, 거기부터 단계적으로 경량화하는 것이 안전하다.
