# Delegate / Work Stealing Review

## 1. 상태
- 이 문서는 현재 active 구현 리뷰가 아니라 역사 리뷰다.
- `2026-04-06` mailbox 리팩터링 이후 기존 delegate / work stealing 코드는 runtime에서 제거됐다.

## 2. 당시 시도했던 것
이전 시도는 아래 두 축을 분리하려 했다.
- `work stealing`
  - one-shot delegated task
- `delegate`
  - content instance migration

그리고 이를 worker-global queue, pending / replay, route commit state machine으로 묶어 구현했다.

## 3. 드러난 문제
실제 추적과 부하 테스트에서 아래 문제가 반복적으로 나왔다.
- same-worker move backlog
- cross-worker route gap
- premature pending
- callback 수명 버그
- lock 순서 역전 가능성
- pending / replay / rollback 복잡도 증가

핵심은 기능 자체보다 구조 전제가 맞지 않았다는 점이다.
- queue 소유권이 worker에 있고
- route는 session에 붙어 있고
- migration은 content instance 단위로 일어나려 하니
정확성과 설명 가능성이 계속 나빠졌다.

## 4. 최종 판단
- 현재 active runtime에서 delegate / work stealing은 제거하는 것이 맞다.
- 먼저 `content-owned mailbox + worker executor` 구조를 기준선으로 고정한다.
- 이후 실제 부하에서 다시 필요하면 mailbox owner 전환 모델로 새로 설계한다.

## 5. 미래에 다시 본다면
미래의 방향은 아래여야 한다.
- `work stealing`
  - state를 직접 건드리지 않는 one-shot task만 대상
- `delegate`
  - worker queue replay가 아니라 mailbox consumer ownership 전환

즉 이 문서의 결론은
- 예전 시도가 틀렸다는 기록
- future load-balancing은 mailbox 전제를 유지해야 한다는 교훈
으로 이해하는 것이 맞다.
