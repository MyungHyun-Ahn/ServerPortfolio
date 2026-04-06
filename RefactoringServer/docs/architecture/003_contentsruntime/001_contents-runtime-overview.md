# ContentsRuntime 개요

## 0. 최신 상태
- `2026-04-06` 기준 `ContentsRuntime`는 `content-owned mailbox + worker executor` 구조다.
- 예전 `content instance = dedicated thread` 구조는 제거됐다.
- `Enter / Leave / Packet`은 이제 worker 공용 큐가 아니라 각 content instance의 mailbox에 적재된다.
- worker는 mailbox를 소비하고 `OnFrame()`을 실행하는 실행자다.
- 이전에 시도했던 `delegate / work stealing` 경로는 현재 코드에서 제거됐고, 관련 문서는 역사 기록으로만 남아 있다.

## 1. 역할
- `ContentsRuntime`는 `NetworkLib`와 개별 content 구현체 사이의 실행 계층이다.
- 책임은 아래와 같다.
  - `sessionId -> contentInstanceId` route 관리
  - content instance 등록과 worker 배치
  - session enter / leave / move 실행
  - packet을 현재 route에 맞는 content mailbox로 전달
  - frame tick 실행

## 2. 핵심 타입
- [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.h)
  - 전체 registry, route table, worker pool을 관리한다.
- [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.h)
  - 이름은 `Thread`지만 의미는 content worker executor다.
  - 여러 content instance의 mailbox를 소비한다.
- [ContentRuntimeTypes.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Core\ContentRuntimeTypes.h)
  - lifecycle event, packet envelope, stats, config를 정의한다.
- [IContentBridge.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Bridge\IContentBridge.h)
  - content가 runtime과 상호작용하는 브리지다.

## 3. 배치 모델
- `FContentRuntime::Start()`는 `ContentsWorkerThreadCount` 기준으로 worker를 만든다.
- 등록된 `contentInstanceId`를 정렬한 뒤 `round-robin`으로 worker에 배치한다.
- 각 `SContentSlot`은 아래를 가진다.
  - `contentId`
  - `contentInstanceId`
  - `content`
  - `workerIndex`
  - `worker`

즉 현재 모델은
- `content instance`가 queue와 상태를 소유하고
- `worker`는 그 queue를 소비하는 실행자다.

## 4. mailbox 모델
- 각 content instance는 자신의 mailbox를 가진다.
- 물리 queue는 `std::deque<SQueuedWorkItem*>` 하나다.
- 논리적으로는 아래 세 종류가 들어간다.
  - `Enter`
  - `Leave`
  - `Packet`
- `enterQueueDepth`, `leaveQueueDepth`, `packetQueueDepth`는 별도 물리 큐가 아니라 통계 카운터다.

worker 쪽에는 아래만 남는다.
- ready content id queue
- worker thread
- content registry
- frame scheduler

즉 예전처럼 worker-global `Enter / Leave / Packet` 큐를 중심으로 움직이지 않는다.

## 5. worker 동작
- worker loop는 크게 두 일을 한다.
  - ready content mailbox 소비
  - due `OnFrame()` 실행
- mailbox가 처음 non-empty가 되면 해당 `contentInstanceId`가 ready queue에 한 번만 들어간다.
- worker는 content 하나를 잡아 batch로 mailbox를 비우고, 아직 남아 있으면 같은 content를 ready queue에 다시 등록한다.
- 기본 batch 크기는 [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)의 `kMailboxBatchSize = 64`다.

이 구조의 장점:
- content 이동 시 worker-global queue replay가 필요 없다.
- queue 소유권이 content에 붙어 있어서 route/move correctness를 다루기 쉬워진다.
- worker는 executor 역할에 집중할 수 있다.

## 6. enter / leave / packet 흐름
### Enter
1. `EnterSession()` 또는 `EnterSessionToInstance()`가 target instance를 결정한다.
2. route slot을 갱신한다.
3. target content mailbox로 `Enter`를 enqueue한다.

### Leave
1. 현재 route를 읽는다.
2. route slot을 비운다.
3. source content mailbox로 `Leave`를 enqueue한다.

### Packet
1. `EnqueuePacket(sessionId, opcode, payload, length)`가 현재 route를 읽는다.
2. 이동 중이 아니면 현재 target content mailbox로 `Packet`을 enqueue한다.
3. 이동 중이면 packet을 session route의 `pendingPackets`에 hold한다.

## 7. session move 흐름
현재 `MoveSessionToInstanceWithCompletion()`은 아래 규칙을 따른다.

### 공통
- route를 바로 target으로 바꾸지 않는다.
- 먼저 route를 `Pending` 상태로 둔다.
- `pendingTargetContentId`, `pendingTargetContentInstanceId`, `pendingTargetWorker`, `pendingTargetRouteGeneration`을 session route에 기록한다.
- 이 구간에서 들어오는 packet은 old route로 보내지 않고 `pendingPackets`에 hold한다.

### cross-worker move
1. source content mailbox에 `Leave`
2. target content mailbox에 `Enter`
3. target enter completion callback에서 route commit
4. `pendingPackets` replay

### same-worker move
- [FContentThread.cpp](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.cpp)의 `EnqueueMoveTransition(...)`을 사용한다.
- 현재 구현은 별도 transition queue를 두지 않고
  - `source Leave` completion에서
  - `target Enter` enqueue
  - 그리고 target enter completion에서 route commit
순서를 보장한다.

이 흐름 덕분에 이전 same-worker backlog / route 선반영 문제를 줄였다.

## 8. frame 실행
- 각 content는 `GetTargetFps()`를 가진다.
- worker는 `nextFrameTime`을 content별로 관리한다.
- due frame만 `OnFrame(delayFrame, bridge)`를 호출한다.
- `delayFrame`, `frameCount`, slow frame 로그 같은 계측도 per-content 기준으로 남는다.

## 9. 현재 빠진 것
- active `delegate / work stealing` lane
- worker-global queue 기반 migration
- content instance를 worker 사이에서 빈번히 옮기는 동적 로드밸런싱

이 방향은 현재 코드에서 제거됐다. 이후 다시 필요하면 `content-owned mailbox` 전제를 유지한 채, mailbox consumer ownership 전환 방식으로만 검토한다.

## 10. 검증 상태
- `250세션 / connectsPerSecond=10 / interval=0 / room-change=90% / hold=180s` 3분 런 성공
  - [content_mailbox_250x3m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x3m_room90)
- `250세션 / connectsPerSecond=10 / interval=0 / room-change=90% / hold=600s` 10분 런 성공
  - [content_mailbox_250x10m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x10m_room90)
- 조건
  - `IOCP`
  - `Server Worker 4`
  - `Contents Worker 4`
  - `Client Worker 4`
  - `Room 77 OnFrame Sleep 15ms`

현재 결론은:
- mailbox 구조가 기존 delegate/move 경로보다 훨씬 단순하고 안정적이다.
- `ContentsRuntime`의 현재 기준 구조는 `content-owned mailbox + worker executor`로 보는 것이 맞다.
