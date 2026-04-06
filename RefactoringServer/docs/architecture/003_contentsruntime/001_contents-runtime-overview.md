# ContentsRuntime Overview

## 0. 최신 상태
- `2026-04-06` 기준 `ContentsRuntime`의 정식 구조는 `content-owned mailbox + worker executor`다.
- `content instance = dedicated thread` 구조는 제거됐다.
- `delegate / work stealing`도 worker-global queue replay 방식이 아니라 `mailbox owner transfer` 방식으로 구현됐다.

## 1. 핵심 구성요소
### FContentRuntime
- 위치: [FContentRuntime.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Routing\FContentRuntime.h)
- 역할
  - content registry
  - session route table
  - worker pool 생성과 배치
  - enter / leave / move / packet 진입 처리
  - owner transfer request / commit

### FContentThread
- 위치: [FContentThread.h](D:\Project\ServerPortfolio\RefactoringServer\ContentsRuntime\Threading\FContentThread.h)
- 역할
  - worker thread 1개를 돌리는 executor
  - ready content의 mailbox drain
  - due frame 실행
  - boundary에서 transfer commit

### SContentExecutionState
- content instance가 실제로 소유하는 실행 상태
- 포함
  - mailbox
  - frame timing
  - queue depth
  - `inFlightCallbackCount`
  - `ownerWorkerIndex`
  - `requestedTransferTargetWorkerIndex`
  - transfer 관련 timestamp와 통계

### SSessionRoute
- session의 현재 route와 move pending 상태를 가진다.
- 포함
  - `contentId`
  - `contentInstanceId`
  - `workerIndex`
  - `worker`
  - `moveState`
  - `pendingTarget*`
  - `pendingPackets`

## 2. queue / mailbox 모델
- `Enter / Leave / Packet`은 worker-global queue가 아니라 content mailbox에 쌓인다.
- worker는 ready queue에서 content를 골라 그 mailbox를 batch로 비운다.
- `enterQueueDepth`, `leaveQueueDepth`, `packetQueueDepth`는 별도 물리 queue가 아니라 mailbox 내부 work 종류별 depth 통계다.

## 3. 배치 모델
- `FContentRuntime::Start()`에서 `ContentsWorkerThreadCount` 기준으로 worker를 만든다.
- 등록된 content instance는 기본적으로 round-robin으로 worker에 배치된다.
- worker는 실행자이고, mailbox와 실행 상태의 실소유자는 content instance다.

## 4. session 흐름
### Enter
1. target content instance 결정
2. route 설정
3. target mailbox에 `Enter` enqueue

### Leave
1. 현재 route 조회
2. route 정리
3. source mailbox에 `Leave` enqueue

### Packet
1. current route 조회
2. move 중이 아니면 current mailbox에 `Packet` enqueue
3. move 중이면 `pendingPackets`에 hold

## 5. move 흐름
### 공통 규칙
- route를 즉시 target으로 바꾸지 않는다.
- 먼저 `Pending` 상태를 만들고 target metadata만 route에 기록한다.
- move 중 packet은 `pendingPackets`에 hold했다가 route commit 뒤 replay한다.

### cross-worker move
1. source `Leave`
2. target `Enter`
3. target enter completion callback에서 route commit
4. held packet replay

### same-worker move
- fast-path를 사용한다.
- source leave와 target enter를 일반 경로보다 짧게 연결하지만, route commit 시점은 여전히 target enter completion 기준이다.

## 6. owner transfer 기반 delegate / work stealing
### 공통 원칙
- 두 기능 모두 queue를 옮기지 않는다.
- mailbox consumer인 owner worker만 바꾼다.
- 실제 commit은 work boundary에서만 일어난다.

### delegate
- 바쁜 source worker가 특정 content를 다른 worker로 push한다.

### work stealing
- idle worker가 바쁜 worker의 content 하나를 pull한다.

### 보호 규칙
- allowList content만 대상
- 중복 request 금지
- request / commit cooldown
- source worker stabilization window
- pending move source/target content는 transfer 금지

## 7. 안전성 규칙
- single-consumer handoff
- boundary-only commit
- move pending packet hold / replay
- pending move source/target content transfer 금지

이 규칙으로
- `move route commit committed=0`
- pending packet 고착
- move와 transfer 동시 충돌
경로를 막는다.

## 8. 검증 기준
- `250 sessions`
- `connectsPerSecond=10`
- `interval=0`
- `room-change=90%`
- `Room 77 OnFrame Sleep 15ms`

성공 런:
- mailbox baseline 10분
  - [content_mailbox_250x10m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x10m_room90)
- owner transfer policy 3분
  - [owner_transfer_policy_loaded_3m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_3m)
- owner transfer policy 10분
  - [owner_transfer_policy_loaded_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_10m)

## 9. 결론
- 현재 `ContentsRuntime`는 `mailbox 중심 구조`로 이해하는 것이 맞다.
- worker는 queue owner가 아니라 executor다.
- `delegate / work stealing`도 이제 mailbox owner transfer 위에서 안전하게 동작하는 확장 기능으로 정리됐다.
