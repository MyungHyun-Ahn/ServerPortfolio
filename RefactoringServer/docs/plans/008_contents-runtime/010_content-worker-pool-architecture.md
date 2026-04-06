# Content Worker Pool / Mailbox Architecture

## 0. 상태
- `2026-04-06` 기준 이 계획은 구현 완료 상태다.
- 초기 목표였던 `content instance = dedicated thread` 제거는 끝났다.
- 최종 구현은 단순 worker-pool을 넘어 `content-owned mailbox + worker executor` 구조로 정리됐다.

## 1. 배경
이전 구조는 사실상 아래와 같았다.
- `Auth 1개 -> thread 1개`
- `Lobby 1개 -> thread 1개`
- `Room N개 -> thread N개`

문제:
- `roomCount` 증가가 그대로 thread 증가로 이어졌다.
- 네트워크 백엔드 비교값이 content thread 수에 크게 흔들렸다.
- 이후 delegate / migration을 붙이려 하자 worker-global queue replay와 route commit 타이밍 문제가 계속 커졌다.

## 2. 최종 선택한 구조
### 2-1. 핵심 원칙
- `content instance`가 mailbox를 소유한다.
- `worker`는 mailbox를 소비하는 실행자다.
- `Enter / Leave / Packet`은 항상 해당 content mailbox에 적재된다.
- worker는 ready content 목록을 돌며 mailbox batch 처리와 frame tick만 담당한다.

### 2-2. 구성
- `FContentRuntime`
  - content registry
  - session route table
  - worker pool
- `FContentThread`
  - worker thread 1개
  - ready content id queue
  - per-content state registry
- `SPerContentState`
  - content pointer
  - `frameDuration`, `nextFrameTime`
  - mailbox
  - per-content stats

## 3. mailbox 모델
- 물리 queue는 content instance마다 하나다.
- queue item 종류
  - `Enter`
  - `Leave`
  - `Packet`
- `enterQueueDepth`, `leaveQueueDepth`, `packetQueueDepth`는 별도 큐가 아니라 종류별 통계 카운터다.
- ready queue는 worker가 가진다.
  - mailbox가 처음 non-empty가 될 때 해당 `contentInstanceId`를 ready queue에 넣는다.
  - worker는 mailbox를 batch로 소비한 뒤 남은 work가 있으면 같은 content를 ready queue에 다시 등록한다.

## 4. worker 배치
- 초기 배치는 `contentInstanceId` 정렬 후 `round-robin`이다.
- `ContentsWorkerThreadCount`가 worker 수를 결정한다.
- slot에는 `workerIndex`, `worker*`만 기록하고, dedicated thread 포인터는 두지 않는다.

이 시점 목표는 placement 최적화보다 `instance-thread` 결합 해소였다.

## 5. session route
`SSessionRoute`는 아래를 가진다.
- `sessionId`
- `routeGeneration`
- `contentId`
- `contentInstanceId`
- `workerIndex`
- `worker`
- `moveState`
- `pendingTarget*`
- `pendingPackets`

즉 route는 이제
- 현재 어느 content instance에 붙어 있는지
- 이동이 pending인지
- pending 중 packet을 어디에 hold하고 있는지
까지 함께 표현한다.

## 6. move 처리
### 6-1. 공통
- move 시작 시 route를 바로 target으로 commit하지 않는다.
- 먼저 `Pending` 상태로 두고 target metadata만 pending 필드에 넣는다.
- 이동 중 들어오는 packet은 old route로 보내지 않고 `pendingPackets`에 hold한다.
- target enter completion 시점에 route commit 후 replay한다.

### 6-2. same-worker fast-path
- source와 target이 같은 worker이면 `EnqueueMoveTransition(...)`을 사용한다.
- 현재 구현은 별도 transition queue가 아니라
  - source leave completion
  - target enter enqueue
  - target enter completion에서 route commit
순서를 보장하는 얇은 fast-path다.

이 방식은 예전 same-worker backlog와 route 선반영 문제를 줄이기 위한 안전한 최소 수정이다.

## 7. delegate / work stealing 계획 변경
- 기존에는 worker-global queue를 전제로 delegate / work stealing을 붙이려 했다.
- 하지만 실제 추적에서 아래 문제가 반복됐다.
  - same-worker move backlog
  - cross-worker route gap
  - pending/replay 복잡도
  - callback 수명/락 순서 문제
- 그래서 현재 active 코드 경로에서는 delegate / work stealing을 제거했다.
- 미래에 다시 필요하면 mailbox owner 전환 모델을 전제로 새 계획으로 다시 잡는다.

## 8. 검증
- `Debug x64` 전체 빌드 성공
- 3분 회귀 성공
  - [content_mailbox_250x3m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x3m_room90)
- 10분 안정성 성공
  - [content_mailbox_250x10m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x10m_room90)
- 조건
  - `250세션`
  - `connectsPerSecond=10`
  - `interval=0`
  - `room-change=90%`
  - `Room 77 OnFrame Sleep 15ms`
  - `Server / Contents / Client Worker = 4 / 4 / 4`

## 9. 남은 과제
- bootstrap control path 우선순위 큐 필요 여부 검토
- worker 배치 정책 고도화
- 추가 content 타입 확장
- mailbox 기반 dynamic migration이 정말 필요한지 재평가

## 10. 결론
- 이번 전환의 핵심은 단순 worker-pool 도입이 아니라 queue 소유권을 worker에서 content로 옮긴 것이다.
- 현재 `ContentsRuntime`의 기준 구조는 `worker queue 중심`이 아니라 `content mailbox 중심`으로 보는 것이 맞다.
