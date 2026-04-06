# Content Worker Pool / Mailbox Review

## 1. 목적
- `ContentsRuntime`가 `content instance = dedicated thread` 구조에서 벗어나 `content-owned mailbox + worker executor` 구조로 바뀐 현재 동작을 정리한다.
- 이 문서는 아래 질문에 답하는 것을 목표로 한다.
  - runtime은 content를 어떻게 배치하는가
  - worker는 실제로 무엇을 소유하는가
  - `Enter / Leave / Packet`은 어디에 쌓이는가
  - session move와 route commit은 어떤 순서로 일어나는가

## 2. 요약
현재 구조의 핵심은 아래 한 줄이다.
- queue 소유권은 worker가 아니라 content instance에 있다.

즉:
- worker는 executor다
- content instance는 mailbox owner다

이 변경으로 예전 worker-global queue 기반 migration과 replay 문제가 크게 줄었다.

## 3. 핵심 타입
### FContentRuntime
- 위치: [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.h)
- 역할
  - content registry
  - session route table
  - worker pool 관리
  - enter / leave / move / packet 진입점 제공

### FContentThread
- 위치: [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.h)
- 이름은 `Thread`지만 현재 의미는 worker executor다.
- 여러 content instance의 mailbox를 소비한다.

### SContentSlot
- `contentId`
- `contentInstanceId`
- `content`
- `workerIndex`
- `worker`

### SSessionRoute
- `sessionId`
- `routeGeneration`
- `contentId`
- `contentInstanceId`
- `workerIndex`
- `worker`
- `moveState`
- `pendingTarget*`
- `pendingPackets`

## 4. worker가 가진 것과 content가 가진 것
### worker가 가진 것
- OS thread 1개
- ready content id queue
- content registry
- condition variable
- pending work 총량 통계

### content가 가진 것
- mailbox
- frame timing
- per-content 통계
- 자신의 `Enter / Leave / Packet` work item들

### 중요
- `enterQueueDepth`, `leaveQueueDepth`, `packetQueueDepth`는 별도 물리 큐가 아니다.
- 실제 물리 큐는 content mailbox 하나고, 종류별 depth는 카운터다.

## 5. mailbox 구조
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)
- `SPerContentState` 안에 `SMailbox`가 있다.
- `SMailbox`
  - `std::mutex`
  - `std::deque<SQueuedWorkItem*>`
  - `readyQueued`

동작:
1. `Enter / Leave / Packet` enqueue 시 해당 content mailbox에 push
2. mailbox가 처음 non-empty가 되면 `readyContentIds`에 그 content id를 한 번만 등록
3. worker가 ready queue에서 content를 꺼내 mailbox를 batch로 소비
4. 아직 남은 work가 있으면 그 content를 ready queue에 다시 등록

## 6. 배치 정책
- `FContentRuntime::Start()`에서 worker를 만든다.
- 등록된 content instance를 `contentInstanceId` 정렬 후 `round-robin`으로 배치한다.
- 현재 배치는 단순하지만, 목적은 먼저 `instance-thread` 결합을 끊는 것이었다.

## 7. enter / leave / packet 흐름
### Enter
1. `EnterSession()` 또는 `EnterSessionToInstance()`
2. route 갱신
3. target content mailbox에 `Enter` enqueue

### Leave
1. 현재 route 조회
2. route slot 비우기
3. source content mailbox에 `Leave` enqueue

### Packet
1. `EnqueuePacket()`에서 현재 route 조회
2. 이동 중이 아니면 현재 content mailbox에 `Packet` enqueue
3. 이동 중이면 session route의 `pendingPackets`에 hold

## 8. move 흐름
### 공통
- route는 move 시작 즉시 target으로 바꾸지 않는다.
- 먼저 `Pending` 상태로 두고 target metadata만 route에 저장한다.
- 이동 중 들어오는 packet은 old route로 보내지 않고 `pendingPackets`에 hold한다.
- target enter completion 시 route commit 후 replay한다.

### same-worker fast-path
- `sourceWorker == targetWorker`이면 [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)의 `EnqueueMoveTransition(...)`을 쓴다.
- 현재 구현은 별도 transition queue 없이
  - source leave
  - source leave completion에서 target enter enqueue
  - target enter completion에서 route commit
순서를 보장한다.

이건 이전 same-worker backlog와 route 선반영 문제를 줄이기 위한 최소 안전 수정이다.

## 9. frame 실행
- `RegisterContent()` 시 `GetTargetFps()`를 읽어 `frameDuration`을 정한다.
- worker는 due frame만 `OnFrame(delayFrame)`으로 실행한다.
- frame이 느릴 경우 `worker slow frame` 로그와 `lastDelayFrame / maxDelayFrame` 통계가 쌓인다.

## 10. 왜 이전 구조보다 나아졌는가
- queue ownership이 content 쪽에 붙어 있어 move correctness를 설명하기 쉽다.
- worker-global queue replay가 필요 없다.
- same-worker와 cross-worker를 구분하더라도 핵심 queue는 동일하게 content mailbox다.
- future migration이 필요해도 mailbox consumer ownership 전환 쪽으로 사고할 수 있다.

## 11. 검증
### 3분 런
- [content_mailbox_250x3m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x3m_room90)
- 조건
  - `250세션`
  - `connectsPerSecond=10`
  - `interval=0`
  - `room-change=90%`
  - `holdSeconds=180`
- 결과
  - 클라이언트 성공 종료
  - `echo validation succeeded`

### 10분 런
- [content_mailbox_250x10m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x10m_room90)
- 조건
  - `IOCP`
  - `Server Worker 4`
  - `Contents Worker 4`
  - `Client Worker 4`
  - `250세션`
  - `connectsPerSecond=10`
  - `interval=0`
  - `room-change=90%`
  - `holdSeconds=600`
  - `Room 77 OnFrame Sleep 15ms`
- 결과
  - 클라이언트 성공 종료
  - `client.err.log` 비어 있음
  - `server.err.log` 비어 있음
  - 종료 시 `enqueueFailTPS=0`, `roomQueue=0`

## 12. 남은 과제
- bootstrap control path 우선순위 분리 필요 여부 검토
- worker 배치 정책 고도화
- 추가 content 타입 확장
- mailbox 기반 future migration이 정말 필요한지 재평가

## 13. 결론
- `content worker pool` 전환의 진짜 핵심은 thread 수 감축만이 아니라 queue 소유권 이동이다.
- 현재 `ContentsRuntime`는 `worker queue 중심`이 아니라 `content mailbox 중심` 구조로 이해하는 것이 맞다.
