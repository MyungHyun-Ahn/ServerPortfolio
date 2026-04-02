# ContentsRuntime 계측 계획

## 1. 목적
- `ContentsRuntime` 내부 상태를 수치로 볼 수 있게 해서 구조 안정성과 병목을 판단하기 쉽게 만든다.
- 이후 큐 최적화나 콘텐츠 확장 작업 전에 기준 지표를 확보한다.

## 2. 계측 대상

### 2.1 FContentThread
- `enterTPS`
- `leaveTPS`
- `packetTPS`
- `frameTPS`
- 현재 queue 길이
  - `enterQueueDepth`
  - `leaveQueueDepth`
  - `packetQueueDepth`
- 최대 queue 길이
  - `maxEnterQueueDepth`
  - `maxLeaveQueueDepth`
  - `maxPacketQueueDepth`
- 프레임 지연
  - `lastDelayFrame`
  - `maxDelayFrame`

### 2.2 FContentRuntime
- 현재 등록 콘텐츠 수
- 현재 활성 세션 수
- 콘텐츠별 세션 수
- `MoveSession` 호출 수
- enqueue 실패 수

## 3. 노출 방식

### 3.1 서버 콘솔
- 기존 `EchoStats` 출력에 `ContentsRuntime` 통계를 함께 출력
- 예시
```text
[ContentStats] contents=2 sessions=1 authSessions=0 echoSessions=1 packetTPS=6 moveTPS=1 echoPacketQueue=0 echoMaxPacketQueue=2 echoMaxDelayFrame=1
```

### 3.2 코드 구조
- `ContentsRuntime::Core::SContentThreadStats`
- `ContentsRuntime::Core::SContentRuntimeStats`
- `FContentThread::GetStatsSnapshot()`
- `FContentRuntime::GetStatsSnapshot()`

## 4. 구현 원칙
- 계측은 구조 이해와 운영성 목적이다.
- hot path를 과하게 느리게 만들지 않도록 원자 카운터와 스냅샷 방식으로 간다.
- 디버그/릴리즈 공통 사용 가능하도록 하되, 문자열 조합은 출력 지점에서만 한다.

## 5. 1차 구현 범위
- `enter/leave/packet/frame` 처리량
- queue 길이와 최대 길이
- 콘텐츠별 세션 수
- `MoveSession` 수
- `maxDelayFrame`

## 6. 검증 기준
- `Login -> Echo` 단발 경로에서 콘텐츠별 세션 수가 맞게 나온다.
- `holdSeconds=1` 반복 경로에서 `packetTPS`가 증가한다.
- 큐가 비정상적으로 계속 쌓이는 경우 콘솔에서 바로 보인다.

## 7. 후속 확장
- 콘텐츠별 처리 시간 평균/최대
- 프레임 처리 시간
- 장시간 soak 테스트용 CSV 출력
- 운영용 관리자 명령 또는 외부 모니터링 연결
