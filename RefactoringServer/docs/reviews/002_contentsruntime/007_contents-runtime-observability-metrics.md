# ContentsRuntime 관측 지표 설명

## 1. 목적
- `[ContentStats]`에 찍히는 값이 무엇을 의미하는지 빠르게 읽을 수 있게 정리한다.

## 2. 핵심 지표

### 2.1 runtime 수준
- `contents`
  - 등록된 콘텐츠 개수
- `sessions`
  - 현재 활성 세션 수
- `moveTPS`
  - 초당 세션 이동 수
- `enqueueFailTPS`
  - 초당 packet enqueue 실패 수
- `runtimeEnqueueLockUs`
  - runtime packet enqueue lock wait 누적값
- `runtimeEnqueueMaxLockUs`
  - runtime packet enqueue 최대 wait

### 2.2 content thread 수준
- `authSessions`, `echoSessions`
  - 각 콘텐츠에 현재 붙어 있는 세션 수
- `authPacketTPS`, `echoPacketTPS`
  - 각 콘텐츠에서 초당 처리한 packet 수
- `echoFrameTPS`
  - 초당 `OnFrame` 호출 수
- `authQueue`, `echoQueue`
  - 현재 packet queue depth
- `authMaxQueue`, `echoMaxQueue`
  - 관측 중 최대 packet queue depth
- `echoLastDelayFrame`
  - 최근 프레임 지연 정도
- `echoMaxDelayFrame`
  - 관측 중 최대 프레임 지연

## 3. 이상 신호 예시
- `enqueueFailTPS > 0`
  - 세션 라우팅 문제 또는 종료 경합 의심
- `echoQueue`가 계속 증가
  - 소비 속도 부족
- `echoMaxDelayFrame`이 크게 증가
  - 콘텐츠 스레드가 프레임 목표를 못 맞춤
- `runtimeEnqueueLockUs`가 비정상적으로 빠르게 증가
  - runtime ingress lock 경합 가능성 높음

## 4. 읽는 순서 추천
1. `enqueueFailTPS`
2. `sessions`
3. `echoQueue / echoMaxQueue`
4. `runtimeEnqueueLockUs`
5. `echoLastDelayFrame / echoMaxDelayFrame`

## 5. 비고
- 성능 비교 결과는 항상 테스트 조건과 함께 기록해야 한다.
- 장시간 soak 결과와 짧은 race injection 결과는 분리해서 해석하는 것이 좋다.
