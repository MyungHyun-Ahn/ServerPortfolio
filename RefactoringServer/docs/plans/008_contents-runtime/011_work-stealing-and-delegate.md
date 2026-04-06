# ContentsRuntime Work Stealing / Delegate 계획

## 0. 현재 상태
- 이 문서는 현재 active plan이 아니라 보류 문서다.
- `2026-04-06` mailbox 리팩터링 이후 기존 `delegate / work stealing` 코드는 runtime에서 제거됐다.

## 1. 왜 보류됐는가
기존 시도는 `worker-global queue`를 전제로 아래를 붙이려는 방향이었다.
- delegated task queue
- idle worker stealing
- content instance migration
- pending / replay / route commit state machine

실제 추적에서 드러난 문제:
- same-worker move backlog
- cross-worker route gap
- premature pending
- callback 수명 버그
- worker queue replay 복잡도
- route와 queue ownership이 분리되어 correctness 설명이 어려움

즉 문제는 기능을 더 붙일수록 구조적 복잡도가 커진다는 점이었다.

## 2. 현재 결정
- active runtime 경로에서는 delegate / work stealing을 제거한다.
- 우선 `content-owned mailbox + worker executor` 구조를 기준선으로 고정한다.
- 실제 부하에서 진짜 load-balancing 필요가 다시 확인될 때만 재검토한다.

## 3. 미래에 다시 검토한다면
재도입의 전제는 아래와 같다.
- `Enter / Leave / Packet`은 계속 content mailbox가 소유한다.
- worker는 mailbox consumer일 뿐이다.
- migration이 필요하면
  - worker queue replay가 아니라
  - mailbox consumer ownership 전환 방식으로 설계한다.

즉 미래의 delegate는 예전처럼
- worker 큐 아이템을 옮기는 모델이 아니라
- content mailbox owner를 바꾸는 모델이어야 한다.

## 4. work stealing 재정의
미래에 다시 쓴다면 의미는 아래처럼 좁히는 것이 맞다.
- `work stealing`
  - one-shot task만 대상
  - content state를 직접 변경하지 않는 보조 계산 lane
- `delegate`
  - content instance 자체를 옮기는 것이라면 mailbox owner 전환으로만 정의

## 5. 현 시점 결론
- 지금 필요한 것은 delegate 자체가 아니라 mailbox baseline 유지와 control-path 정리다.
- 따라서 이 계획은 삭제하지 않고 기록으로 남기되, 현재 구현 대상으로는 간주하지 않는다.
