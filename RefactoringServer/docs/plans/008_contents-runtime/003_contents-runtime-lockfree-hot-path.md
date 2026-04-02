# ContentsRuntime 락 프리화 전 hot path 분석 계획

## 1. 목적
- `ContentsRuntime` 전체를 무리하게 락 프리로 바꾸기 전에, 실제 병목이 되는 구간을 먼저 찾는다.
- 락 프리화 후보를 `hot path`와 `non-hot path`로 나눠서 우선순위를 정한다.
- 이후 구현은 "전체 락 프리"가 아니라 "효과가 큰 경로부터 단계적으로 경량화"하는 방향으로 간다.

## 2. 전제
- 현재 `ContentsRuntime`는 `std::mutex + std::condition_variable + std::deque` 기반이다.
- 구조 검증과 기능 검증은 이미 끝났고, `Login -> Chat snapshot -> Echo` 경로도 통과했다.
- 따라서 지금 단계의 핵심 질문은
  - "어디가 가장 자주 호출되는가"
  - "어디가 lock contention을 만들 가능성이 큰가"
  - "어디는 락 프리로 바꿔도 되고, 어디는 정합성 때문에 락을 유지해야 하는가"
  를 구분하는 것이다.

## 3. 분석 대상 경로

### 3.1 콘텐츠 스레드 inbox enqueue
- 대상 코드
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentThread.cpp)
  - `EnqueueEnter`
  - `EnqueueLeave`
  - `EnqueuePacket`
- 특징
  - 네트워크 worker 또는 상위 호출 스레드가 자주 밀어 넣는다.
  - 특히 `EnqueuePacket`은 recv packet마다 호출되므로 가장 강한 hot path 후보다.
- 1차 판단
  - 락 프리화 최우선 후보

### 3.2 콘텐츠 스레드 loop dequeue
- 대상 코드
  - [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentThread.cpp)
  - `wait_until`
  - queue swap
  - packet 처리 루프
- 특징
  - 소비자는 콘텐츠 스레드 하나다.
  - producer는 여러 개일 수 있으므로 MPSC 패턴과 잘 맞는다.
- 1차 판단
  - `packetQueue`를 MPSC lock-free queue로 바꾸는 경우 함께 재설계 필요

### 3.3 sessionId -> contentId 매핑
- 대상 코드
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentRuntime.cpp)
  - `EnterSession`
  - `LeaveSession`
  - `EnqueuePacket`
  - `MoveSession`
- 특징
  - `EnqueuePacket`마다 조회가 들어간다.
  - 동시에 `MoveSession`, `LeaveSession`, disconnect가 정합성을 요구한다.
- 1차 판단
  - 자주 읽히는 경로이지만, 정합성 리스크가 높다.
  - 당장은 완전 락 프리보다
    - sharded lock
    - slot array
    - session slot 기반 direct index
    같은 중간 단계가 더 현실적일 수 있다.

### 3.4 콘텐츠 이동 경로
- 대상 코드
  - [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\FContentRuntime.cpp)
  - `MoveSession`
- 특징
  - packet enqueue보다 빈도는 낮을 가능성이 크다.
  - 하지만 잘못 최적화하면 `Leave -> Enter` 순서 보장이 깨질 수 있다.
- 1차 판단
  - 락 프리 우선순위 낮음
  - 정합성 우선

### 3.5 Start / Stop / RegisterContent
- 대상 코드
  - `RegisterContent`
  - `Start`
  - `Stop`
- 특징
  - 운영 중 hot path가 아니다.
- 1차 판단
  - 락 프리화 대상에서 제외

## 4. 우선순위 분류

### 4.1 1순위: 락 프리 후보
- `FContentThread::EnqueuePacket`
- 필요 시 `enter`, `leave`까지 포함한 이벤트 inbox 통합

### 4.2 2순위: 경량화 후보
- `sessionId -> contentId` 조회 경로
- 방향 후보
  - direct slot lookup
  - shard 분할
  - read-heavy 구조 최적화

### 4.3 3순위: 락 유지 권장
- `MoveSession`
- `Stop`
- `RegisterContent`

## 5. 추천 기술 방향

### 5.1 이벤트 큐 통합
- 현재는 `enterQueue`, `leaveQueue`, `packetQueue`가 분리되어 있다.
- 이후 검토안:
  - `SContentEvent`
    - `Enter`
    - `Leave`
    - `Packet`
  - 하나의 MPSC queue로 통합
- 장점
  - enqueue 경로 단순화
  - 락 프리 queue 적용 지점이 한 군데로 줄어듦

### 5.2 packet queue만 먼저 교체
- 가장 보수적인 접근
- `enter/leave`는 지금 락 기반 유지
- `packetQueue`만 lock-free MPSC queue로 교체
- 장점
  - 효과가 큰 hot path에 집중 가능
  - 콘텐츠 이동 정합성 영향이 작음

### 5.3 session mapping은 즉시 락 프리화하지 않음
- 이 경로는 병목일 수도 있지만, 정합성 리스크가 더 크다.
- 먼저 계측 결과로 정말 contention이 큰지 보고 판단한다.

## 6. 측정 기준
- 이미 추가한 `ContentsRuntime` 통계를 활용한다.
- 추가로 보면 좋은 것
  - `packetQueueDepth`의 평균/최대
  - `enqueueFailTPS`
  - `echoPacketTPS`
  - `delayFrame`
- 이후 lock-free 후보 검증 시 비교 기준
  - TPS 변화
  - queue depth 변화
  - CPU 변화
  - 장시간 안정성

## 7. 구현 단계 제안
1. 현재 계측으로 baseline 수집
2. `packetQueue`가 실제로 hot path인지 로그/부하로 확인
3. `packetQueue`만 MPSC 락 프리 queue 프로토타입 적용
4. 반복/장시간 테스트
5. 필요하면 `enter/leave` 통합 이벤트 큐로 확장
6. 그다음에야 `session mapping` 경량화 검토

## 8. 결론
- `ContentsRuntime`를 지금 바로 전면 락 프리화하는 것은 비추천이다.
- 가장 유력한 hot path는 `FContentThread::EnqueuePacket`이다.
- 따라서 다음 구현은
  - 먼저 hot path를 계측으로 확인하고
  - `packet inbox`부터 락 프리화하는 방향이 가장 안전하고 효과적이다.
