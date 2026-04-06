# Delegate / Move Failure Trace Review

## 1. 목적
- `delegate / migration` 시도 과정에서 어떤 문제가 있었는지와, 최종적으로 어떤 수정으로 정리됐는지 남긴다.
- 현재 active 구조의 배경 문서이자 root cause 기록이다.

## 2. 초기 재현 조건
- `250 sessions`
- `connectsPerSecond=10`
- `interval=0`
- `room-change=90%`
- `RecvTimeoutMs=30000`
- `Room 77 OnFrame Sleep 15ms`

목표는 content backlog 아래에서 move correctness와 transfer correctness를 동시에 보는 것이었다.

## 3. 추적 중 확인된 문제
### 3-1. same-worker move backlog
- same-worker room move도 일반 queue backlog에 묻혀 `RoomChangeRp`가 늦어질 수 있었다.
- same-worker fast-path를 추가해 이 경로를 줄였다.

### 3-2. route 선반영 문제
- target enter 전에 route를 먼저 바꾸면 packet이 target room으로 너무 일찍 들어가 stale처럼 보일 수 있었다.
- deferred route commit으로 수정했다.

### 3-3. pending packet hold 필요성
- move 중 들어온 packet을 old route로 바로 보내면 cross-worker gap이 생겼다.
- `pendingPackets` hold 후 commit 뒤 replay로 수정했다.

### 3-4. 가장 중요한 버그
- `pending move source content`에 owner transfer가 들어가고 있었다.
- 이 경우 move completion callback은 old `sourceWorker / sourceWorkerIndex` 가정으로 route commit을 시도한다.
- 하지만 transfer가 먼저 source 또는 pendingTarget의 worker cache를 바꾸면 `move route commit ... committed=0`가 된다.
- 이후 held packet replay가 일어나지 않아 timeout으로 이어졌다.

## 4. 최종 수정
- pending move의 source content transfer 금지
- pending move의 target content transfer 금지
- 이 검사를
  - transfer request 시점
  - boundary commit 시점
  둘 다에서 수행

이 수정으로 move와 owner transfer가 같은 content를 동시에 commit하는 경로를 막았다.

## 5. 결과
### 가드 적용 후 스모크
- [owner_transfer_policy_smoke_3m_pendingmove_guard](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_smoke_3m_pendingmove_guard)
- `move route commit ... committed=0` 재현 없음
- client 성공 종료

### loaded 검증
- [owner_transfer_policy_loaded_3m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_3m)
- [owner_transfer_policy_loaded_10m](D:\Project\ServerPortfolio\RefactoringServer\Out\owner_transfer_policy_loaded_10m)
- 실제 transfer가 반복돼도 성공 종료

## 6. 역사적 결론
- 이 문서가 보여주는 핵심은 “delegate를 조금만 다듬으면 된다”가 아니었다.
- 핵심은 queue ownership과 consumer ownership을 분리해야 correctness가 단순해진다는 점이었다.
- 현재 구조는 그 교훈 위에 정리된 결과다.
