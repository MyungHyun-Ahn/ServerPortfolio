# Content Worker Pool / Mailbox Review

## 1. 목적
- 현재 `ContentsRuntime`가 실제로 어떻게 동작하는지 정리한다.
- 특히 `worker가 무엇을 소유하고`, `content instance가 무엇을 소유하는지`를 명확히 남긴다.

## 2. 핵심 결론
- 현재 구조의 중심은 `worker queue`가 아니라 `content mailbox`다.
- worker는 executor다.
- content instance가 mailbox, frame state, queue depth, 실행 통계를 소유한다.

## 3. 왜 이 구조로 갔는가
이전 구조의 문제는 이랬다.
- `content instance = dedicated thread`
- worker-global queue 기반 move/replay
- same-worker / cross-worker 이동마다 보정 로직 증가
- route 선반영, backlog, replay correctness 문제가 반복

mailbox 구조로 바꾼 뒤 좋아진 점은 이렇다.
- queue ownership이 content에 붙는다.
- move correctness를 설명하기 쉬워진다.
- worker는 executor 역할에 집중한다.
- 이후 owner transfer도 queue replay 없이 consumer handoff로 설명 가능하다.

## 4. 실제 소유권
### worker가 소유하는 것
- OS thread
- ready content queue
- worker-local wakeup / condition variable
- 대략적인 pending work 통계

### content instance가 소유하는 것
- mailbox
- `Enter / Leave / Packet` item
- frame timing
- queue depth 통계
- `inFlightCallbackCount`
- owner transfer 관련 상태

## 5. 흐름 정리
### Enter / Leave / Packet
- 모두 target content mailbox에 enqueue된다.
- worker는 ready queue에서 content를 골라 mailbox를 batch로 비운다.

### Move
- route는 즉시 target으로 바뀌지 않는다.
- `Pending` 상태와 `pendingTarget*` metadata만 기록한다.
- move 중 packet은 `pendingPackets`에 hold한다.
- target enter completion에서 route를 commit하고 held packet을 replay한다.

### Same-worker move
- fast-path를 사용한다.
- 그래도 route commit 시점은 target enter completion 이후로 유지한다.

## 6. 이번 구조에서 중요해진 것
- `pendingPackets`
- deferred route commit
- same-worker fast-path
- worker cache는 보조값이고, authoritative state는 runtime/content slot 쪽이라는 점

## 7. 안정성 검증
### mailbox baseline
- [content_mailbox_250x10m_room90](D:\Project\ServerPortfolio\RefactoringServer\Out\content_mailbox_250x10m_room90)
- 조건
  - `250 sessions`
  - `connectsPerSecond=10`
  - `interval=0`
  - `room-change=90%`
  - `holdSeconds=600`
  - `Room 77 OnFrame Sleep 15ms`
- 결과
  - client 성공 종료
  - server/client error log 없음

## 8. 리뷰 결론
- `ContentsRuntime`는 현재 `content-owned mailbox + worker executor` 구조로 보는 것이 맞다.
- worker pool 전환의 본질은 thread 수 감소가 아니라 queue ownership 이전이었다.
- 이후 load balancing을 다시 붙이더라도 이 구조를 기준으로 가야 한다.
