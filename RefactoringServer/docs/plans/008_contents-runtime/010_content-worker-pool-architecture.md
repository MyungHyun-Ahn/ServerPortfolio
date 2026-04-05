# Content Worker Pool Architecture

## 0. Implementation Update
- `2026-04-06` 기준 1차 구현이 반영됐다.
- `FContentRuntime`는 worker pool을 만들고, content instance를 `round-robin`으로 배치한다.
- `FOwnedPacketEnvelope`, `SContentLifecycleEvent`는 `contentInstanceId`를 직접 들고 worker queue에 들어간다.
- `EchoServer`에는 `ContentsWorkerThreadCount` 설정이 추가됐다.
- 검증:
  - `Debug x64` 솔루션 빌드 성공
  - `room-count=80`, `contents-worker-thread-count=4`에서 서버 process thread count `10`
  - `20 sessions / 10s` 스모크 성공
  - `100 sessions / 30s` 짧은 회귀 성공

## 1. 목적
- 현재 `ContentsRuntime`의 `content instance = dedicated thread` 결합 구조를 해소한다.
- 레거시 `CContentsThread` 기반처럼 `content worker pool` 위에 여러 content instance를 배치하는 구조로 바꾼다.
- `Auth`, `Lobby`, `Room` 수가 늘어날 때 스레드 수가 그대로 증가하는 문제를 줄이고, 이후 `NetworkLib` 성능 비교 결과가 콘텐츠 스레드 수에 덜 오염되도록 만든다.

## 2. 현재 문제
현재 구조:
- [FContentRuntime.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.cpp)
  - `RegisterContent()`된 instance마다 [FContentThread](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.h) 하나를 붙인다.
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)
  - 각 instance가 자기 worker thread, queue, frame loop를 직접 가진다.

즉 현재는 사실상:
- `Auth 1개 -> thread 1개`
- `Lobby 1개 -> thread 1개`
- `Room N개 -> thread N개`

문제:
- `roomCount` 증가가 곧 스레드 수 증가로 이어진다.
- `instance` 수와 CPU core 수가 쉽게 분리되지 않는다.
- `NetworkLib`의 `IOCP / RIO / SO_SNDBUF` 비교가 콘텐츠 스레드 수에 크게 영향을 받는다.
- 레거시의 `contents thread pool` 모델과 의미가 다르다.

## 3. 레거시 기준
레거시 참고:
- [CContentsThread.h](D:\Project\ServerPortfolio\NetworkLib\CContentsThread.h)
- [CContentsThread.cpp](D:\Project\ServerPortfolio\NetworkLib\CContentsThread.cpp)
- [CBaseContents.cpp](D:\Project\ServerPortfolio\NetworkLib\CBaseContents.cpp)

핵심 의미:
- 콘텐츠 처리용 스레드는 풀로 존재한다.
- 여러 content가 같은 worker thread 위에 올라간다.
- `instance != thread`
- work stealing / delegate 같은 후속 최적화도 thread pool 기준으로 얹어진다.

이번 리팩터링은 레거시의 모든 세부 구현을 복제하기보다, 이 핵심 의미를 현재 `ContentsRuntime` 구조에 맞게 복원하는 것이 목적이다.

## 4. 목표 구조
### 4-1. 큰 그림
- `FContentRuntime`
  - content instance registry
  - session route table
  - `content worker pool`
  - `contentInstanceId -> worker` 배치 정보
- `FContentWorker`
  - 실제 OS thread 하나 소유
  - 여러 content instance의 enter / leave / packet / frame 작업 처리
- `IContent`
  - 여전히 instance 단위 객체
  - 더 이상 자기 전용 thread를 소유하지 않음

### 4-2. 핵심 원칙
- `content instance`와 `worker thread`를 분리한다.
- worker 수는 설정 기반 고정값으로 두고, instance는 worker에 배치한다.
- 같은 session의 현재 content instance가 어느 worker에 배치되어 있는지 route가 알아야 한다.
- `MoveSession()`은 source worker와 target worker가 서로 다를 수 있다는 것을 전제로 동작해야 한다.

## 5. 제안 구조
### 5-1. 새 타입
- `ContentsRuntime/Threading/FContentWorker.h/.cpp`
  - 하나의 worker thread
  - 여러 instance용 work item 큐
  - frame tick 루프
- `ContentsRuntime/Threading/FContentWorkerPool.h/.cpp`
  - worker 생성/시작/정지
  - 배치 정책
- `FContentThread`
  - 제거 대상
  - 또는 과도기엔 thin wrapper로 남기되 최종적으로는 `FContentWorker`로 치환

### 5-2. slot 구조 변경
현재:
- `SContentSlot`
  - `content`
  - `thread`

목표:
- `SContentSlot`
  - `content`
  - `assignedWorkerIndex`
  - `worker`

### 5-3. session route 변경
현재:
- route가 `thread` 포인터를 직접 들고 있음

목표:
- route가 `contentId`, `contentInstanceId`, `workerIndex`, `worker*`를 가진다.
- enqueue는 route의 worker로 간다.

## 6. 배치 정책
### 6-1. 1차 정책
- worker 수는 config로 받는다.
- content instance 등록 순서대로 `round-robin` 배치한다.

이유:
- 1차 목적은 `instance-thread` 결합 해소다.
- 세션 수/부하 기반 동적 재배치는 후순위다.
- 고정 배치가 라우팅과 디버깅이 가장 단순하다.

### 6-2. 후속 정책 후보
- room count가 커질 때 room instance만 별도 spread
- active session count 기반 재배치
- work stealing / delegate

이번 범위에는 넣지 않는다.

## 7. frame 처리 정책
현재는 instance별 `FContentThread`가 자기 `GetTargetFps()` 기준으로 frame loop를 돌린다.

worker pool 구조에선 다음 중 하나가 필요하다.

1. worker가 자기에게 배치된 content instance를 순회하며 frame 검사
2. instance별 next frame 시각을 worker timer queue에 등록

1차 구현은 `worker timer queue`보다 단순한 순회 방식으로 시작한다.
- worker는 자기에게 배치된 instance 목록을 가지고 있음
- frame tick에서 각 instance의 target fps / next frame time을 검사
- frame이 필요한 instance만 `OnFrame()` 호출

## 8. enqueue 모델
worker queue에 들어가는 항목:
- `Enter`
- `Leave`
- `Packet`
- 필요하면 `Frame`

work item은 최소한 아래를 포함한다.
- `kind`
- `contentInstanceId`
- `sessionId`
- `routeGeneration`
- payload 또는 lifecycle event

중요:
- `worker queue`는 `worker 단일 소비자` 구조를 유지한다.
- 여러 instance가 같은 worker를 공유해도 queue는 worker 기준 하나다.

## 9. config 변경
새 설정 추가:
- `ContentsWorkerThreadCount`

적용 방향:
- `EchoServer.yaml`에서 기본값을 명시
- `SContentRuntimeConfig`에 필드 추가
- `EchoServer/Main.cpp`에서 config 매핑

초기 권장값:
- 테스트 머신 기준 `min(physical/logical core, 4~8)` 범위
- 현재 10코어 PC라면 1차 기준 `4` 또는 `6`부터 검증

## 10. 전환 단계
### 10-1. 1단계
- `FContentWorker`, `FContentWorkerPool` 추가
- `FContentRuntime`가 worker pool을 시작/정지
- content slot에 `worker`를 배치

### 10-2. 2단계
- `EnterSession`, `LeaveSession`, `EnqueuePacket`, `MoveSession`이 worker 기준으로 enqueue되게 변경
- `FContentThread` 참조 제거

### 10-3. 3단계
- `GetStatsSnapshot()`을 worker pool 기준으로 재정의
- 기존 `SContentThreadStats`는 이름 유지 여부를 검토
  - 필요하면 `SContentWorkerStats`로 분리

### 10-4. 4단계
- `Auth / Lobby / Room` 흐름 회귀
- `100세션 / 3분`
- `250세션 / 10분`
- room change 경로 포함 검증

## 11. 검증 기준
필수 검증:
- `EchoServer + EchoClient` 1세션 스모크
- `Lobby -> Room -> RoomChange -> Echo` 정상 동작
- `100세션 / 3분` 회귀
- `250세션 / 10분` 회귀
- 기존 `ContentsRuntime` race injection 옵션이 여전히 동작하는지 확인

추가로 봐야 할 것:
- 콘텐츠 스레드 수 감소 확인
- CPU 사용률 변화
- enqueue lock wait / queue depth 변화

## 12. 주의사항
- `MoveSession()`은 source/target이 같은 worker인지 다른 worker인지 모두 안전해야 한다.
- content instance 배치가 고정되므로, route와 slot의 worker 정보가 어긋나면 안 된다.
- 기존 문서와 코드에서 `thread`라고 부르던 개념을 `worker`로 정리해야 한다.
- 이번 범위에서 레거시의 work stealing / delegate까지 한 번에 가져오지 않는다.

## 13. 현재 판단
- 이 리팩터링은 `ContentsRuntime` 후속 확장 항목이 아니라 선행 구조 정리 항목이다.
- `NetworkLib` 성능 비교를 계속하기 전에 먼저 반영하는 것이 맞다.
- 우선순위는 `008 ContentsRuntime`를 다시 `진행 중`으로 올릴 정도로 높다.
