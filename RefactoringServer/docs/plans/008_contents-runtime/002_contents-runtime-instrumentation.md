# ContentsRuntime 계측 계획

## 1. 목적
- `ContentsRuntime` 내부 상태를 수치로 보이게 해서 구조 안정성과 병목 위치를 빠르게 판단한다.
- 이후 queue 경량화나 lock-free inbox 검증의 기준선을 만든다.

## 2. 수집 대상
### 2.1 `FContentThread`
- `enterCount`
- `leaveCount`
- `packetCount`
- `frameCount`
- 현재 queue depth
  - `enterQueueDepth`
  - `leaveQueueDepth`
  - `packetQueueDepth`
- 최대 queue depth
  - `maxEnterQueueDepth`
  - `maxLeaveQueueDepth`
  - `maxPacketQueueDepth`
- enqueue lock wait
  - `enqueueEnterLockWaitNs`
  - `enqueueLeaveLockWaitNs`
  - `enqueuePacketLockWaitNs`
- 최대 lock wait
  - `maxEnqueueEnterLockWaitNs`
  - `maxEnqueueLeaveLockWaitNs`
  - `maxEnqueuePacketLockWaitNs`
- 프레임 지연
  - `lastDelayFrame`
  - `maxDelayFrame`

### 2.2 `FContentRuntime`
- `registeredContentCount`
- `activeSessionCount`
- `enterSessionCallCount`
- `leaveSessionCallCount`
- `enqueuePacketCallCount`
- `moveSessionCount`
- `enqueueFailureCount`
- runtime lock wait
  - `enterSessionLockWaitNs`
  - `leaveSessionLockWaitNs`
  - `enqueuePacketLockWaitNs`
  - `moveSessionLockWaitNs`
- 최대 runtime lock wait
  - `maxEnterSessionLockWaitNs`
  - `maxLeaveSessionLockWaitNs`
  - `maxEnqueuePacketLockWaitNs`
  - `maxMoveSessionLockWaitNs`

## 3. 노출 방식
### 3.1 서버 headless 콘솔
- 기존 `EchoStats`와 같이 `[ContentStats]` 한 줄을 출력한다.
- 예시
```text
[ContentStats] contents=2 sessions=1 moveTPS=1 enqueueFailTPS=0 authSessions=0 echoSessions=1 echoPacketTPS=3 echoQueue=0 echoMaxQueue=1 echoLastDelayFrame=1 echoMaxDelayFrame=1
```

### 3.2 스냅샷 구조
- `SContentThreadStats`
- `SContentRuntimeStats`
- `GetStatsSnapshot()`

## 4. 해석 기준
- `enqueueFailTPS > 0`
  - 세션 라우팅 오류, shutdown 경쟁, 잘못된 콘텐츠 ID 가능성
- `packetQueueDepth` 지속 증가
  - 콘텐츠 소비 속도가 ingress보다 느리다
- `maxDelayFrame` 지속 증가
  - 콘텐츠 프레임 루프가 밀리고 있다
- `enqueuePacketLockWaitNs`가 다른 항목보다 크게 누적
  - packet ingress가 hot path일 가능성이 높다

## 5. 참고 문서
- 지표 해석 기준은 [007_contents-runtime-observability-metrics.md](D:\Project\ServerPortfolio\RefactoringServer\docs\reviews\002_contentsruntime\007_contents-runtime-observability-metrics.md)에 정리한다.

## 6. 후속 작업
- CSV 출력
- 장시간 soak 결과 자동 축적
- 콘텐츠 처리 시간 계측
