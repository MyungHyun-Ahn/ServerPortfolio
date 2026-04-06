# Delegate Migration / Backlog Trace Review

## 1. 상태
- 이 문서는 현재 구조의 active 문제 문서라기보다, 이전 delegate / migration 시도에서 무엇이 문제였는지 남기는 역사 리뷰다.
- 현재 runtime은 mailbox 구조로 전환됐고, 이 문서에서 추적한 직접 경로는 active 코드에서 제거됐다.

## 2. 당시 재현 조건
- `250세션`
- `interval=0`
- `room-change=90%`
- `RecvTimeoutMs=30000`
- `Room 77 OnFrame Sleep 15ms`
- `Contents Worker 4`

목적은 강한 content backlog 아래에서 delegate / move correctness를 보는 것이었다.

## 3. 당시 확인된 핵심 문제
### 3-1. same-worker move backlog
- `sourceWorker == targetWorker`인 room-to-room move에서도
  - `leave`
  - `enter`
  - `completion`
이 일반 worker queue 뒤로 밀렸다.
- 결과적으로 `move accepted=1` 이후에도 `RoomChangeRp`가 timeout 날 수 있었다.

### 3-2. route 선반영 문제
- target enter 전에 route를 먼저 바꾸면
  - packet이 새 room으로 너무 일찍 라우팅되고
  - 아직 `OnEnter()`가 안 된 target room에서 stale처럼 보일 수 있었다.

### 3-3. cross-worker route gap
- move commit 전후 창에서 bootstrap / control packet이 old route 또는 replay 경로에 걸려 유실처럼 보이는 증상이 있었다.

### 3-4. 구현 복잡도
- pending / replay / rollback
- callback lifetime
- lock ordering
- delegate storm 억제 정책
이 한꺼번에 얽혀 구조가 지나치게 복잡해졌다.

## 4. 그 과정에서 넣었던 수정
delegate 경로를 살리기 위해 당시 아래 수정이 순차적으로 들어갔다.
- route deferred commit
- pending move packet buffer
- same-worker move fast-path
- migration transaction화
- cooldown / stabilization window
- connect ramp-up

일부 증상은 줄었지만, 전체 구조 복잡도는 계속 증가했다.

## 5. 최종 해석
이 추적의 가장 큰 수확은 특정 버그 하나보다 구조 판단이다.
- worker-global queue를 중심으로 delegate / migration을 얹는 방향은 유지 비용이 너무 컸다.
- queue 소유권을 content로 옮겨야 move correctness를 단순하게 설명할 수 있었다.

즉 이 문서의 결론은
- delegate를 조금 더 튜닝하면 해결된다는 뜻이 아니라
- mailbox 구조로 다시 정리하는 편이 맞았다는 근거다.

## 6. 실제 해결 방향
최종적으로 active 코드 경로는 아래로 바뀌었다.
- `content-owned mailbox`
- worker는 executor
- move 중 packet은 session route의 `pendingPackets`에 hold
- route는 target enter completion 시점에 commit
- same-worker move는 얇은 fast-path만 유지
- delegate / work stealing active 경로 제거

## 7. 현재 상태
mailbox 전환 후 기준 안정성 런은 아래 조건에서 통과했다.
- `250세션`
- `connectsPerSecond=10`
- `interval=0`
- `room-change=90%`
- `holdSeconds=600`
- `Room 77 OnFrame Sleep 15ms`
- 결과 경로: [content_mailbox_250x10m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x10m_room90)

즉 이 문서는 더 이상 현재 병목 문서가 아니라,
`왜 delegate / migration 경로를 걷어내고 mailbox 구조로 갔는가`를 설명하는 기록으로 보는 것이 맞다.
