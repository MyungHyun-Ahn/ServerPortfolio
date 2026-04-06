# Mailbox Owner Transfer 기반 Delegate / Work Stealing

## 0. 상태
- `2026-04-06` 기준 구현 완료
- 현재 `ContentsRuntime`의 정식 방향은 `content-owned mailbox + worker executor` 위에 `mailbox owner transfer`를 얹는 구조다.

## 1. 목적
- `delegate`와 `work stealing`을 별도 worker-global queue 시스템으로 만들지 않는다.
- 둘 다 `content mailbox의 owner worker를 바꾸는 기능`으로 정의한다.
- 차이는 trigger만 다르다.
  - `delegate`: 바쁜 source worker가 push
  - `work stealing`: 한가한 target worker가 pull

## 2. 기본 전제
- queue는 worker가 아니라 `content instance`가 소유한다.
- worker는 queue를 소비하는 executor다.
- 따라서 transfer는 queue item replay보다 `consumer ownership handoff` 문제로 다뤄야 한다.

## 3. 현재 구현 구조
### 3-1. mailbox / execution state
- `Enter / Leave / Packet`은 content instance mailbox에 쌓인다.
- frame timing, queue depth, `inFlightCallbackCount`, transfer request 상태도 content execution state가 가진다.
- worker는 ready content를 골라 mailbox를 batch로 drain하고 frame을 실행한다.

### 3-2. owner transfer 메커니즘
- runtime은 `requestedTransferTargetWorkerIndex`만 기록한다.
- 실제 transfer commit은 callback/frame 도중이 아니라 `work boundary`에서만 수행한다.
- commit 시:
  - old worker detach
  - target worker attach
  - slot의 `worker / workerIndex` 갱신
  - execution state의 `ownerWorkerIndex` 갱신
  - session route의 worker cache 갱신

### 3-3. move와의 관계
- session move는 `pending move` 상태를 먼저 만들고 route commit은 target enter completion 시점에 한다.
- move 중 들어온 packet은 `pendingPackets`에 hold했다가 commit 뒤 replay한다.
- same-worker move는 fast-path를 사용한다.

## 4. 안전 규칙
### 4-1. boundary-only commit
- transfer는 `OnEnter / OnLeave / OnPacket / OnFrame` 실행 중간에 commit하지 않는다.
- 현재 work를 끝낸 직후 boundary에서만 commit한다.

### 4-2. single-consumer handoff
- 한 시점에 mailbox consumer는 하나뿐이다.
- old worker가 detach된 뒤에만 target worker가 이어서 consume한다.

### 4-3. pending move content transfer 금지
- `moveState == Pending`인 session이 source 또는 pending target으로 걸려 있는 content는 transfer 후보에서 제외한다.
- 이 검사는
  - request 시점
  - commit 시점
  둘 다에서 수행한다.
- 이 가드가 `move route commit committed=0`와 `pendingPackets` 고착을 막는 핵심 수정이다.

### 4-4. same-worker / cross-worker 공통성
- queue 소유권은 content에 있고, transfer는 consumer만 바꾸므로 same-worker와 cross-worker를 동일한 원칙으로 다룬다.

## 5. 정책
### 5-1. delegate
- source worker pending work가 높고
- 특정 content의 queue depth 또는 frame delay가 일정 수준 이상 지속되면
- 가장 한가한 worker로 transfer request를 건다.

### 5-2. work stealing
- idle worker가 가장 바쁜 worker를 보고
- queue depth가 큰 content 하나를 골라 pull request를 건다.

### 5-3. 보호 장치
- allowList 기반 content 제한
  - 현재는 `Room`만 허용
- request cooldown
- commit cooldown
- source worker stabilization window
- 이미 request가 걸린 content는 중복 request 금지

## 6. 구현 메모
- worker-global replay queue는 사용하지 않는다.
- transfer request는 lightweight metadata만 기록한다.
- 실제 detach/register는 boundary commit에서만 수행한다.
- move pending route와 owner transfer가 충돌하지 않도록 session route를 스캔하는 가드를 둔다.

## 7. 검증
### 7-1. 조건
- `250 sessions`
- `connectsPerSecond=10`
- `interval=0`
- `room-change=90%`
- `Server Worker 4`
- `Contents Worker 4`
- `Client Worker 4`
- `Room 77 OnFrame Sleep 15ms`

### 7-2. 결과
- `3분` loaded run 성공
  - [owner_transfer_policy_loaded_3m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_3m)
- `10분` loaded run 성공
  - [owner_transfer_policy_loaded_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_10m)
- 두 런 모두
  - 실제 `delegate request scheduled`
  - 실제 `work steal request scheduled`
  - 실제 `owner transfer commit`
  이 반복적으로 발생했다.
- `move route commit committed=0`는 재현되지 않았다.
- client/server error log도 비어 있었다.

## 8. 결론
- `delegate / work stealing`은 이제 mailbox 구조 위에서 동작하는 실구현으로 본다.
- 현재 수준에선 `ContentsRuntime` 기본 구조를 신뢰 가능한 상태로 본다.
- 앞으로의 작업은 구조 교체보다는 정책 튜닝, 관측 지표 보강, 더 긴 soak 검증이다.
